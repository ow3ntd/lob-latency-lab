#include "OrderBook.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
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

struct ClockProfile {
    std::size_t samples = 0;
    std::size_t zero_delta_reads = 0;
    std::uint64_t smallest_nonzero_ns = 0;
};

struct BatchResult {
    std::size_t events_processed = 0;
    std::size_t trades_generated = 0;
    std::size_t batch_size = 0;
    std::vector<std::uint64_t> batch_total_ns;
    std::vector<double> per_event_ns;
};

constexpr std::size_t default_event_count = 1'000'000;
constexpr std::size_t default_batch_size = 128;
constexpr std::size_t warmup_event_count = 2'000'000;
constexpr std::size_t cycle_length = 4;
constexpr std::size_t clock_probe_samples = 200'000;

// Measures the practical resolution of steady_clock on this machine.
// Two back-to-back reads bound how small a single measurable interval can be.
ClockProfile profile_clock(std::size_t samples) {
    ClockProfile profile;
    profile.samples = samples;
    std::uint64_t smallest = std::numeric_limits<std::uint64_t>::max();

    for (std::size_t i = 0; i < samples; ++i) {
        const auto first = std::chrono::steady_clock::now();
        const auto second = std::chrono::steady_clock::now();
        const auto delta =
            std::chrono::duration_cast<std::chrono::nanoseconds>(second - first).count();

        if (delta == 0) {
            ++profile.zero_delta_reads;
        } else {
            smallest = std::min(smallest, static_cast<std::uint64_t>(delta));
        }
    }

    profile.smallest_nonzero_ns =
        (smallest == std::numeric_limits<std::uint64_t>::max()) ? 0 : smallest;
    return profile;
}

std::size_t parse_size_arg(const std::string& value, const char* name) {
    std::size_t parsed = 0;
    const auto result = static_cast<std::size_t>(std::stoull(value, &parsed));

    if (parsed != value.size()) {
        throw std::runtime_error(std::string(name) + " must be a positive integer");
    }

    return result;
}

std::vector<BenchmarkEvent> generate_events(std::size_t event_count) {
    std::vector<BenchmarkEvent> events;
    events.reserve(event_count);

    lob::OrderId next_order_id = 1;
    lob::Timestamp timestamp = 1;

    while (events.size() < event_count) {
        const bool buy_cross_cycle = (events.size() / cycle_length) % 2 == 0;

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

void process_event(lob::OrderBook& book,
                   const BenchmarkEvent& event,
                   std::size_t& trades_generated) {
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

// Times a whole batch with one pair of clock reads, then divides by the batch
// size. Each sample therefore sits well above the clock's resolution floor.
// This measures the mean per-event cost within a batch, not the latency of any
// individual event -- batching smooths the per-event tail by construction.
BatchResult run_batched_benchmark(const std::vector<BenchmarkEvent>& events,
                                  std::size_t batch_size) {
    lob::OrderBook book;
    BatchResult result;
    result.batch_size = batch_size;

    const std::size_t batch_count = events.size() / batch_size;
    result.batch_total_ns.reserve(batch_count);
    result.per_event_ns.reserve(batch_count);

    for (std::size_t batch = 0; batch < batch_count; ++batch) {
        const std::size_t begin = batch * batch_size;
        const std::size_t end = begin + batch_size;

        std::atomic_signal_fence(std::memory_order_seq_cst);
        const auto start = std::chrono::steady_clock::now();
        std::atomic_signal_fence(std::memory_order_seq_cst);

        for (std::size_t i = begin; i < end; ++i) {
            process_event(book, events[i], result.trades_generated);
        }

        std::atomic_signal_fence(std::memory_order_seq_cst);
        const auto stop = std::chrono::steady_clock::now();
        std::atomic_signal_fence(std::memory_order_seq_cst);

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();

        result.batch_total_ns.push_back(static_cast<std::uint64_t>(elapsed));
        result.per_event_ns.push_back(static_cast<double>(elapsed) /
                                      static_cast<double>(batch_size));
        result.events_processed += batch_size;
    }

    return result;
}

// The former measurement strategy, retained so every run shows the artifact
// side by side with the corrected numbers.
std::vector<double> run_per_event_benchmark(const std::vector<BenchmarkEvent>& events,
                                            std::size_t& trades_generated) {
    lob::OrderBook book;
    std::vector<double> per_event_ns;
    per_event_ns.reserve(events.size());

    for (const auto& event : events) {
        const auto start = std::chrono::steady_clock::now();
        process_event(book, event, trades_generated);
        const auto stop = std::chrono::steady_clock::now();

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
        per_event_ns.push_back(static_cast<double>(elapsed));
    }

    return per_event_ns;
}

double percentile(const std::vector<double>& sorted_values, double percentile_rank) {
    if (sorted_values.empty()) {
        return 0.0;
    }

    const auto index = static_cast<std::size_t>(
        (percentile_rank / 100.0) * static_cast<double>(sorted_values.size() - 1)
    );

    return sorted_values[index];
}

std::uint64_t median_u64(std::vector<std::uint64_t> values) {
    if (values.empty()) {
        return 0;
    }

    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc > 3) {
            throw std::runtime_error(
                "Usage: bench_order_book_latency [event_count] [batch_size]");
        }

        std::size_t event_count = default_event_count;
        std::size_t batch_size = default_batch_size;

        if (argc >= 2) {
            event_count = parse_size_arg(argv[1], "event_count");
        }
        if (argc >= 3) {
            batch_size = parse_size_arg(argv[2], "batch_size");
        }

        if (batch_size == 0 || batch_size % cycle_length != 0) {
            throw std::runtime_error("batch_size must be a positive multiple of 4");
        }

        event_count -= event_count % batch_size;

        if (event_count == 0) {
            throw std::runtime_error("event_count must cover at least one full batch");
        }

        const auto clock_profile = profile_clock(clock_probe_samples);

        auto warmup_events = generate_events(warmup_event_count);
        run_warmup(warmup_events);

        auto measured_events = generate_events(event_count);

        auto batched = run_batched_benchmark(measured_events, batch_size);
        auto batched_sorted = batched.per_event_ns;
        std::sort(batched_sorted.begin(), batched_sorted.end());

        std::size_t naive_trades = 0;
        auto naive = run_per_event_benchmark(measured_events, naive_trades);
        std::sort(naive.begin(), naive.end());

        const auto median_batch_ns = median_u64(batched.batch_total_ns);
        const double ticks_per_batch =
            (clock_profile.smallest_nonzero_ns == 0)
                ? 0.0
                : static_cast<double>(median_batch_ns) /
                      static_cast<double>(clock_profile.smallest_nonzero_ns);

        std::cout << std::fixed << std::setprecision(2);

        std::cout << "Order book per-event cost benchmark\n\n";

        std::cout << "Clock profile\n";
        std::cout << "  zero-delta back-to-back reads: "
                  << clock_profile.zero_delta_reads << " / "
                  << clock_profile.samples << "\n";
        std::cout << "  smallest nonzero delta:        "
                  << clock_profile.smallest_nonzero_ns << " ns\n\n";

        std::cout << "Batched measurement (per-event cost from batch totals)\n";
        std::cout << "  events processed:      " << batched.events_processed << "\n";
        std::cout << "  batch size:            " << batched.batch_size << " events\n";
        std::cout << "  batches:               " << batched.per_event_ns.size() << "\n";
        std::cout << "  trades generated:      " << batched.trades_generated << "\n";
        std::cout << "  median batch duration: " << median_batch_ns << " ns ("
                  << ticks_per_batch << " clock ticks)\n";
        std::cout << "  p50: " << percentile(batched_sorted, 50.0) << " ns/event\n";
        std::cout << "  p95: " << percentile(batched_sorted, 95.0) << " ns/event\n";
        std::cout << "  p99: " << percentile(batched_sorted, 99.0) << " ns/event\n";
        std::cout << "  max: " << batched_sorted.back() << " ns/event\n\n";

        std::cout << "Naive per-event timing (resolution-limited, for comparison)\n";
        std::cout << "  p50: " << percentile(naive, 50.0) << " ns\n";
        std::cout << "  p95: " << percentile(naive, 95.0) << " ns\n";
        std::cout << "  p99: " << percentile(naive, 99.0) << " ns\n";
        std::cout << "  max: " << naive.back() << " ns\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
