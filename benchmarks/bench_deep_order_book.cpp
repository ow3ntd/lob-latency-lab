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
constexpr std::size_t event_cycle_length = 8;
constexpr std::size_t seed_price_levels = 100;
constexpr std::size_t seed_orders_per_level = 20;
constexpr lob::Price midpoint = 10000;
constexpr lob::Quantity resting_quantity = 1000;
constexpr lob::Quantity added_quantity = 100;
constexpr lob::Quantity crossing_quantity = 10;

std::size_t round_down_to_cycle(std::size_t event_count) {
    return event_count - (event_count % event_cycle_length);
}

std::size_t parse_event_count(int argc, char* argv[]) {
    if (argc > 2) {
        throw std::runtime_error("Usage: bench_deep_order_book [event_count]");
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
        throw std::runtime_error("event_count must be at least 8");
    }

    return event_count;
}

std::vector<BenchmarkEvent> generate_seed_events(
    lob::OrderId& next_order_id,
    lob::Timestamp& timestamp,
    std::vector<lob::OrderId>& cancelable_bid_ids,
    std::vector<lob::OrderId>& cancelable_ask_ids
) {
    std::vector<BenchmarkEvent> events;
    events.reserve(seed_price_levels * seed_orders_per_level * 2);

    for (std::size_t level = 0; level < seed_price_levels; ++level) {
        const auto bid_price = midpoint - 1 - static_cast<lob::Price>(level);
        const auto ask_price = midpoint + 1 + static_cast<lob::Price>(level);

        for (std::size_t order = 0; order < seed_orders_per_level; ++order) {
            lob::Order bid{
                next_order_id++,
                lob::Side::Buy,
                bid_price,
                resting_quantity,
                timestamp++
            };

            lob::Order ask{
                next_order_id++,
                lob::Side::Sell,
                ask_price,
                resting_quantity,
                timestamp++
            };

            events.push_back({EventType::Add, bid});
            events.push_back({EventType::Add, ask});

            if (level >= seed_price_levels / 2) {
                cancelable_bid_ids.push_back(bid.id);
                cancelable_ask_ids.push_back(ask.id);
            }
        }
    }

    return events;
}

std::vector<BenchmarkEvent> generate_measured_events(
    std::size_t event_count,
    lob::OrderId& next_order_id,
    lob::Timestamp& timestamp,
    const std::vector<lob::OrderId>& cancelable_bid_ids,
    const std::vector<lob::OrderId>& cancelable_ask_ids
) {
    std::vector<BenchmarkEvent> events;
    events.reserve(event_count);

    std::vector<lob::OrderId> bid_cancel_queue = cancelable_bid_ids;
    std::vector<lob::OrderId> ask_cancel_queue = cancelable_ask_ids;
    std::size_t next_bid_cancel = 0;
    std::size_t next_ask_cancel = 0;
    std::size_t cycle = 0;

    while (events.size() < event_count) {
        const auto deep_bid_level = static_cast<lob::Price>((cycle % (seed_price_levels / 2)) + 51);
        const auto deep_ask_level = static_cast<lob::Price>((cycle % (seed_price_levels / 2)) + 51);

        lob::Order add_bid{
            next_order_id++,
            lob::Side::Buy,
            midpoint - deep_bid_level,
            added_quantity,
            timestamp++
        };

        lob::Order add_ask{
            next_order_id++,
            lob::Side::Sell,
            midpoint + deep_ask_level,
            added_quantity,
            timestamp++
        };

        lob::Order cancel_bid{
            bid_cancel_queue[next_bid_cancel++],
            lob::Side::Buy,
            midpoint - deep_bid_level,
            resting_quantity,
            timestamp++
        };

        lob::Order cancel_ask{
            ask_cancel_queue[next_ask_cancel++],
            lob::Side::Sell,
            midpoint + deep_ask_level,
            resting_quantity,
            timestamp++
        };

        lob::Order crossing_buy{
            next_order_id++,
            lob::Side::Buy,
            midpoint + 1,
            crossing_quantity,
            timestamp++
        };

        lob::Order replenish_ask{
            next_order_id++,
            lob::Side::Sell,
            midpoint + 1,
            added_quantity,
            timestamp++
        };

        lob::Order crossing_sell{
            next_order_id++,
            lob::Side::Sell,
            midpoint - 1,
            crossing_quantity,
            timestamp++
        };

        lob::Order replenish_bid{
            next_order_id++,
            lob::Side::Buy,
            midpoint - 1,
            added_quantity,
            timestamp++
        };

        events.push_back({EventType::Add, add_bid});
        events.push_back({EventType::Add, add_ask});
        events.push_back({EventType::Cancel, cancel_bid});
        events.push_back({EventType::Cancel, cancel_ask});
        events.push_back({EventType::Add, crossing_buy});
        events.push_back({EventType::Add, replenish_ask});
        events.push_back({EventType::Add, crossing_sell});
        events.push_back({EventType::Add, replenish_bid});

        bid_cancel_queue.push_back(add_bid.id);
        ask_cancel_queue.push_back(add_ask.id);
        ++cycle;
    }

    return events;
}

void process_seed_events(lob::OrderBook& book, const std::vector<BenchmarkEvent>& events) {
    for (const auto& event : events) {
        if (event.type == EventType::Add) {
            book.add_order(event.order);
        } else {
            book.cancel_order(event.order.id);
        }
    }
}

BenchmarkResult run_benchmark(
    lob::OrderBook& book,
    const std::vector<BenchmarkEvent>& measured_events
) {
    BenchmarkResult result;

    auto start = std::chrono::steady_clock::now();

    for (const auto& event : measured_events) {
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
        const auto measured_event_count = parse_event_count(argc, argv);

        lob::OrderId next_order_id = 1;
        lob::Timestamp timestamp = 1;
        std::vector<lob::OrderId> cancelable_bid_ids;
        std::vector<lob::OrderId> cancelable_ask_ids;

        auto seed_events = generate_seed_events(
            next_order_id,
            timestamp,
            cancelable_bid_ids,
            cancelable_ask_ids
        );

        auto measured_events = generate_measured_events(
            measured_event_count,
            next_order_id,
            timestamp,
            cancelable_bid_ids,
            cancelable_ask_ids
        );

        lob::OrderBook book;
        process_seed_events(book, seed_events);
        auto result = run_benchmark(book, measured_events);

        const double elapsed_seconds =
            static_cast<double>(result.elapsed.count()) / 1'000'000'000.0;
        const double events_per_second =
            static_cast<double>(result.events_processed) / elapsed_seconds;
        const double average_ns_per_event =
            static_cast<double>(result.elapsed.count()) /
            static_cast<double>(result.events_processed);

        std::cout << "Deep order book benchmark\n";
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
