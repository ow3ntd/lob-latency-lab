#include "OrderBook.hpp"
 
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
 
namespace {
 
struct StressResult {
    std::size_t order_count = 0;
    std::chrono::nanoseconds add_elapsed{0};
    std::chrono::nanoseconds cancel_elapsed{0};
};
 
constexpr lob::Price stress_price = 10050;
 
std::vector<std::size_t> default_order_counts() {
    return {5'000, 10'000, 20'000, 40'000, 80'000};
}
 
std::vector<std::size_t> parse_order_counts(int argc, char* argv[]) {
    if (argc == 1) {
        return default_order_counts();
    }
 
    std::vector<std::size_t> counts;
 
    for (int i = 1; i < argc; ++i) {
        std::string value = argv[i];
        std::size_t parsed = 0;
        auto count = std::stoull(value, &parsed);
 
        if (parsed != value.size() || count == 0) {
            throw std::runtime_error("order_count must be a positive integer");
        }
 
        counts.push_back(static_cast<std::size_t>(count));
    }
 
    return counts;
}
 
StressResult run_stress(std::size_t order_count) {
    lob::OrderBook book;
    StressResult result;
    result.order_count = order_count;
 
    lob::Timestamp timestamp = 1;
 
    auto add_start = std::chrono::steady_clock::now();
 
    for (std::size_t i = 0; i < order_count; ++i) {
        lob::OrderId id = static_cast<lob::OrderId>(i + 1);
        book.add_order({id, lob::Side::Buy, stress_price, 10, timestamp++});
    }
 
    auto add_end = std::chrono::steady_clock::now();
    result.add_elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(add_end - add_start);
 
    auto cancel_start = std::chrono::steady_clock::now();
 
    for (std::size_t i = 0; i < order_count; ++i) {
        lob::OrderId id = static_cast<lob::OrderId>(i + 1);
        book.cancel_order(id);
    }
 
    auto cancel_end = std::chrono::steady_clock::now();
    result.cancel_elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(cancel_end - cancel_start);
 
    return result;
}
 
double average_ns(std::chrono::nanoseconds elapsed, std::size_t operation_count) {
    return static_cast<double>(elapsed.count()) / static_cast<double>(operation_count);
}
 
}  // namespace
 
int main(int argc, char* argv[]) {
    try {
        auto order_counts = parse_order_counts(argc, argv);
 
        std::cout << "Cancel stress benchmark\n";
        std::cout << "Every order rests at the same price level (worst case for a\n";
        std::cout << "position-shifting cancel); all orders are cancelled front-to-back,\n";
        std::cout << "the worst case for an implementation that must shift remaining\n";
        std::cout << "elements on every cancel.\n\n";
 
        std::cout << "orders,add_ns_per_op,cancel_ns_per_op,cancel_total_ms\n";
 
        for (auto order_count : order_counts) {
            auto result = run_stress(order_count);
 
            const double add_ns_per_op = average_ns(result.add_elapsed, order_count);
            const double cancel_ns_per_op = average_ns(result.cancel_elapsed, order_count);
            const double cancel_total_ms =
                static_cast<double>(result.cancel_elapsed.count()) / 1'000'000.0;
 
            std::cout << order_count << ","
                      << add_ns_per_op << ","
                      << cancel_ns_per_op << ","
                      << cancel_total_ms << "\n";
        }
 
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
 