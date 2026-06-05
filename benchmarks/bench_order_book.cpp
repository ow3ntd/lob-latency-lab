#include "OrderBook.hpp"

#include <chrono>
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

struct BenchmarkResult {
    std::size_t events_processed = 0;
    std::size_t trades_generated = 0;
    std::chrono::nanoseconds elapsed{0};
};

constexpr std::size_t default_event_count = 1'000'000;
constexpr std::size_t warmup_event_count = 10'000;

std::size_t parse_event_count(int argc, char* argv[]) {
    if (argc == 1) {
        return default_event_count;
    }

    if (argc != 2) {
        throw std::runtime_error("Usage: bench_order_book [event_count]");
    }

    std::string value = argv[1];
    std::size_t parsed = 0;
    auto event_count = std::stoull(value, &parsed);

    if (parsed != value.size() || event_count == 0) {
        throw std::runtime_error("event_count must be a positive integer");
    }

    return static_cast<std::size_t>(event_count);
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

        if (events.size() < event_count) {
            events.push_back({EventType::Add, cancel_order});
        }

        if (events.size() < event_count) {
            events.push_back({EventType::Add, crossing_order});
        }

        if (events.size() < event_count) {
            events.push_back({EventType::Cancel, cancel_order});
        }
    }

    return events;
}

BenchmarkResult run_benchmark(const std::vector<BenchmarkEvent>& events) {
    lob::OrderBook book;
    BenchmarkResult result;

    auto start = std::chrono::steady_clock::now();

    for (const auto& event : events) {
        if (event.type == EventType::Add) {
            auto trades = book.add_order(event.order);
            result.trades_generated += trades.size();
        } else {
            book.cancel_order(event.order.id);
        }

        ++result.events_processed;
    }

    auto end = std::chrono::steady_clock::now();
    result.elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    return result;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        auto measured_event_count = parse_event_count(argc, argv);

        auto warmup_events = generate_events(warmup_event_count);
        run_benchmark(warmup_events);

        auto measured_events = generate_events(measured_event_count);
        auto result = run_benchmark(measured_events);

        const double elapsed_seconds =
            static_cast<double>(result.elapsed.count()) / 1'000'000'000.0;
        const double events_per_second =
            static_cast<double>(result.events_processed) / elapsed_seconds;
        const double average_ns_per_event =
            static_cast<double>(result.elapsed.count()) /
            static_cast<double>(result.events_processed);

        std::cout << "Order book benchmark\n";
        std::cout << "Events processed: " << result.events_processed << "\n";
        std::cout << "Trades generated: " << result.trades_generated << "\n";
        std::cout << "Elapsed time: " << elapsed_seconds << " seconds\n";
        std::cout << "Events per second: " << events_per_second << "\n";
        std::cout << "Average ns/event: " << average_ns_per_event << "\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
