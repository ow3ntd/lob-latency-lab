#include "MarketDataReplay.hpp"

#include <charconv>
#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace {

std::size_t parse_depth(std::string_view text) {
    std::size_t depth = 0;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto [ptr, error] = std::from_chars(begin, end, depth);

    if (error == std::errc::result_out_of_range) {
        throw std::runtime_error("depth is out of range");
    }
    if (error != std::errc{} || ptr != end || depth == 0) {
        throw std::runtime_error("depth must be a positive integer");
    }

    return depth;
}

std::string_view side_name(lob::LobsterBookSide side) {
    return side == lob::LobsterBookSide::Bid ? "bid" : "ask";
}

void print_level(const std::optional<lob::LevelSnapshot>& level) {
    if (!level) {
        std::cout << "missing";
        return;
    }

    std::cout << "price=" << level->price
              << ", quantity=" << level->quantity;
}

void print_summary(
    std::string_view message_path,
    std::string_view orderbook_path,
    std::size_t depth,
    const lob::LobsterTransitionSummary& summary
) {
    std::cout << "Message file: " << message_path << '\n'
              << "Order-book file: " << orderbook_path << '\n'
              << "Requested depth: " << depth << '\n'
              << "Rows read: " << summary.rows_read << '\n'
              << "Baseline rows: " << summary.baseline_rows << '\n'
              << "Transitions checked: " << summary.transitions_checked << '\n'
              << "Matching transitions: " << summary.matching_transitions << '\n'
              << "Mismatching transitions: "
              << summary.mismatching_transitions << '\n'
              << "Unsupported transitions: "
              << summary.unsupported_transitions << '\n'
              << "Unverifiable transitions: "
              << summary.unverifiable_transitions << '\n'
              << "Tail-refill transitions: "
              << summary.tail_refill_transitions << '\n'
              << "Opposite-side changes: "
              << summary.opposite_side_changes << '\n'
              << "Missing event-price levels: "
              << summary.missing_event_price_levels << '\n'
              << "Quantity underflows: " << summary.quantity_underflows << '\n'
              << "Price mismatches: " << summary.price_mismatches << '\n'
              << "Quantity mismatches: " << summary.quantity_mismatches << '\n'
              << "Missing-level mismatches: "
              << summary.missing_level_mismatches << '\n';

    if (summary.first_mismatch) {
        const lob::LobsterTransitionMismatch& mismatch =
            *summary.first_mismatch;

        std::cout << "First mismatch row: " << mismatch.row << '\n'
                  << "First mismatch event type: " << mismatch.event_type << '\n'
                  << "First mismatch event price: " << mismatch.event_price << '\n'
                  << "First mismatch event size: " << mismatch.event_size << '\n'
                  << "First mismatch side: " << side_name(mismatch.side) << '\n'
                  << "First mismatch level: " << mismatch.level << '\n'
                  << "First mismatch reason: " << mismatch.reason << '\n'
                  << "First mismatch expected: ";
        print_level(mismatch.expected);
        std::cout << '\n' << "First mismatch actual: ";
        print_level(mismatch.actual);
        std::cout << '\n';
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr
            << "Usage: lob_lobster_transition_validate "
               "<message.csv> <orderbook.csv> <depth>\n";
        return 1;
    }

    try {
        const std::size_t depth = parse_depth(argv[3]);

        std::ifstream message_input(argv[1]);
        if (!message_input) {
            std::cerr << "Error: could not open message file: " << argv[1]
                      << '\n';
            return 1;
        }

        std::ifstream orderbook_input(argv[2]);
        if (!orderbook_input) {
            std::cerr << "Error: could not open order-book file: " << argv[2]
                      << '\n';
            return 1;
        }

        const lob::LobsterTransitionSummary summary =
            lob::validate_lobster_transitions(
                message_input,
                orderbook_input,
                depth
            );

        print_summary(argv[1], argv[2], depth, summary);
        return summary.mismatching_transitions == 0 ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
