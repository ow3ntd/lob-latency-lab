#include "OrderBook.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum class EventType {
    Add,
    Cancel
};

struct BenchmarkEvent {
    EventType type;
    lob::Order order;
};

struct LatencyResult {
    std::size_t events_processed = 0;
    std::size_t trades_generated = 0;
    std::vector<std::uint64_t> latencies_ns;
};

constexpr std::size_t default_event_count = 100'000;
constexpr std::size_t warmup_event_count = 10'000;

std::size_t round_down_to_cycle(std::size_t event_count) {
    return event_count - (event_count % 4);
}

std::size_t parse_event_count(int argc, char* argv[]) {
    if (argc > 2) {
        throw std::runtime_error("Usage: bench_order_book_latency [event_count]");
    }

    std::size_t event_count = default_event_count;

    if (argc == 2) {
        std::string value = argv[1];
        std::size_t parsed = 0;
        event_count = static_cast<std::size_t>(std::stoull(value, &parsed));

        if (parsed != value.size()) {
            throw std::runtime_error("event_count must be a positive integer");
        }
    }

    event_count = round_down_to_cycle(event_count);

    if (event_count == 0) {
        throw std::runtime_error("event_count must be at least 4");
    }

    return event_count;
}

std::vector<BenchmarkEvent> generate_events(std::size_t event_count) {
    std::vector<BenchmarkEvent> events;
    events.reserve(event_count);

    lob::OrderId next_order_id = 1;
    lob::Timestamp timestamp = 1;

    while (events.size() < event_count) {
        const bool buy_cross_cycle = (events.size() / 4) % 2 == 0;

        lob::Order resting_order;
        lob::Order crossing_order;
        lob::Order cancel_order;

        if (buy_cross_cycle) {
            resting_order = {next_order_id++, lob::Side::Sell, 10060, 100, timestamp++};
            cancel_order = {next_order_id++, lob::Side::Buy, 10040, 100, timestamp++};
            crossing_order = {next_order_id++, lob::Side::Buy, 10060, 100, timestamp++};
        } else {
            resting_order = {next_order_id++, lob::Side::Buy, 10050, 100, timestamp++};
            cancel_order = {next_order_id++, lob::Side::Sell, 10070, 100, timestamp++};
            crossing_order = {next_order_id++, lob::Side::Sell, 10050, 100, timestamp++};
        }

        events.push_back({EventType::Add, resting_order});
        events.push_back({EventType::Add, cancel_order});
        events.push_back({EventType::Add, crossing_order});
        events.push_back({EventType::Cancel, cancel_order});
    }

    return events;
}

void process_event(lob::OrderBook& book, const BenchmarkEvent& event, std::size_t& trades_generated) {
    if (event.type == EventType::Add) {
        auto trades = book.add_order(event.order);
        trades_generated += trades.size();
    } else {
        book.cancel_order(event.order.id);
    }
}

void run_warmup(const std::vector<BenchmarkEvent>& events) {
    lob::OrderBook book;
    std::size_t trades_generated = 0;

    for (const auto& event : events) {
        process_event(book, event, trades_generated);
    }
}

LatencyResult run_latency_benchmark(const std::vector<BenchmarkEvent>& events) {
    lob::OrderBook book;
    LatencyResult result;
    result.latencies_ns.reserve(events.size());

    for (const auto& event : events) {
        auto start = std::chrono::steady_clock::now();
        process_event(book, event, result.trades_generated);
        auto end = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        result.latencies_ns.push_back(static_cast<std::uint64_t>(elapsed.count()));
        ++result.events_processed;
    }

    return result;
}

std::uint64_t percentile_latency(const std::vector<std::uint64_t>& sorted_latencies, double percentile) {
    auto index = static_cast<std::size_t>(
        (percentile / 100.0) * static_cast<double>(sorted_latencies.size() - 1)
    );

    return sorted_latencies[index];
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        auto measured_event_count = parse_event_count(argc, argv);

        auto warmup_events = generate_events(warmup_event_count);
        run_warmup(warmup_events);

        auto measured_events = generate_events(measured_event_count);
        auto result = run_latency_benchmark(measured_events);

        std::sort(result.latencies_ns.begin(), result.latencies_ns.end());

        std::cout << "Order book latency benchmark\n";
        std::cout << "Events processed: " << result.events_processed << "\n";
        std::cout << "Trades generated: " << result.trades_generated << "\n";
        std::cout << "p50 latency: " << percentile_latency(result.latencies_ns, 50.0) << " ns\n";
        std::cout << "p95 latency: " << percentile_latency(result.latencies_ns, 95.0) << " ns\n";
        std::cout << "p99 latency: " << percentile_latency(result.latencies_ns, 99.0) << " ns\n";
        std::cout << "Max latency: " << result.latencies_ns.back() << " ns\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
