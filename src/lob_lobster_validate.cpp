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
    const char* begin = text.data();
    const char* end = begin + text.size();
    auto result = std::from_chars(begin, end, depth);

    if (result.ec == std::errc::result_out_of_range) {
        throw std::runtime_error("depth is out of range");
    }

    if (result.ec != std::errc{} || result.ptr != end || depth == 0) {
        throw std::runtime_error("depth must be a positive integer");
    }

    return depth;
}

const char* side_name(lob::LobsterBookSide side) {
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
    const lob::LobsterValidationSummary& summary
) {
    std::cout << "Message file: " << message_path << '\n';
    std::cout << "Order-book file: " << orderbook_path << '\n';
    std::cout << "Requested depth: " << depth << '\n';
    std::cout << "Rows compared: " << summary.rows_compared << '\n';
    std::cout << "Matching rows: " << summary.matching_rows << '\n';
    std::cout << "Mismatching rows: " << summary.mismatching_rows << '\n';
    std::cout << "Missing-level mismatches: "
              << summary.missing_level_mismatches << '\n';
    std::cout << "Price mismatches: " << summary.price_mismatches << '\n';
    std::cout << "Quantity mismatches: " << summary.quantity_mismatches << '\n';
    std::cout << "Events processed: " << summary.replay.events_processed << '\n';
    std::cout << "New orders: " << summary.replay.new_orders << '\n';
    std::cout << "Partial cancellations: "
              << summary.replay.partial_cancels << '\n';
    std::cout << "Deletions: " << summary.replay.deletions << '\n';
    std::cout << "Visible executions: " << summary.replay.executions << '\n';
    std::cout << "Successful visible-execution reductions: "
              << summary.replay.successful_execution_reductions << '\n';
    std::cout << "Hidden executions: "
              << summary.replay.hidden_executions << '\n';
    std::cout << "Cross trades: " << summary.replay.cross_trades << '\n';
    std::cout << "Auxiliary events: " << summary.replay.auxiliary << '\n';
    std::cout << "Successful cancellations: "
              << summary.replay.successful_cancels << '\n';
    std::cout << "Executed quantity: "
              << summary.replay.executed_quantity << '\n';

    if (summary.first_mismatch) {
        const auto& mismatch = *summary.first_mismatch;
        std::cout << "First mismatch row: " << mismatch.row << '\n';
        std::cout << "First mismatch side: " << side_name(mismatch.side) << '\n';
        std::cout << "First mismatch level: " << mismatch.level << '\n';
        std::cout << "First mismatch expected: ";
        print_level(mismatch.expected);
        std::cout << '\n';
        std::cout << "First mismatch actual: ";
        print_level(mismatch.actual);
        std::cout << '\n';
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: lob_lobster_validate "
                  << "<message.csv> <orderbook.csv> <depth>\n";
        return 1;
    }

    try {
        std::size_t depth = parse_depth(argv[3]);
        std::ifstream message_input(argv[1]);
        if (!message_input) {
            std::cerr << "Error: could not open message file: " << argv[1] << '\n';
            return 1;
        }

        std::ifstream orderbook_input(argv[2]);
        if (!orderbook_input) {
            std::cerr << "Error: could not open order-book file: " << argv[2] << '\n';
            return 1;
        }

        lob::OrderBook book;
        auto summary = lob::validate_lobster_replay(
            message_input, orderbook_input, book, depth
        );
        print_summary(argv[1], argv[2], depth, summary);
        return summary.mismatching_rows == 0 ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
