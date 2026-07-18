#pragma once

#include "OrderBook.hpp"

#include <cstddef>
#include <istream>
#include <optional>
#include <string_view>
#include <vector>

namespace lob {

struct ReplaySummary {
    std::size_t events_processed = 0;
    std::size_t add_events = 0;
    std::size_t cancel_events = 0;
    std::size_t successful_cancels = 0;
    std::size_t trades_generated = 0;
    Quantity total_traded_quantity = 0;
};

ReplaySummary replay_market_data(std::istream& input, OrderBook& book);

// LOBSTER message-file replay.
//
// LOBSTER 'message' files are 6-column CSVs, one row per order-book event:
//   Time, Type, OrderID, Size, Price, Direction
// where Type is 1=new limit order, 2=partial cancel, 3=deletion,
// 4=visible execution, 5=hidden execution, 6=cross trade,
// 7=trading halt/auxiliary,
// and Direction is 1=buy (bid) side, -1=sell (ask) side.
//
// This reader replays the order-flow events (submissions, cancellations,
// deletions, and visible executions) through the book. LOBSTER executions
// are already resolved in the source data, so type-4 events reduce the
// referenced resting order without being re-matched. Hidden executions,
// cross trades, and halts are counted but do not mutate the visible book.
struct LobsterSummary {
    std::size_t events_processed = 0;                 // total rows read
    std::size_t new_orders = 0;                       // type 1
    std::size_t partial_cancels = 0;                  // type 2
    std::size_t deletions = 0;                        // type 3
    std::size_t executions = 0;                       // type 4
    std::size_t successful_execution_reductions = 0; // successful type-4 reductions
    std::size_t hidden_executions = 0;                // type 5, skipped
    std::size_t cross_trades = 0;                     // type 6, skipped
    std::size_t auxiliary = 0;                        // type 7, skipped
    std::size_t successful_cancels = 0;               // successful type-2/type-3 changes
    Quantity executed_quantity = 0;                   // sum of type-4 sizes
};

LobsterSummary replay_lobster_data(std::istream& input, OrderBook& book);

struct LobsterBookRow {
    std::vector<LevelSnapshot> asks;
    std::vector<LevelSnapshot> bids;

    bool operator==(const LobsterBookRow&) const = default;
};

LobsterBookRow parse_lobster_book_row(
    std::string_view line,
    std::size_t depth
);

enum class LobsterBookSide {
    Bid,
    Ask
};

struct LobsterValidationMismatch {
    std::size_t row = 0;
    LobsterBookSide side = LobsterBookSide::Bid;
    std::size_t level = 0;
    std::optional<LevelSnapshot> expected;
    std::optional<LevelSnapshot> actual;
};

struct LobsterValidationSummary {
    std::size_t rows_compared = 0;
    std::size_t matching_rows = 0;
    std::size_t mismatching_rows = 0;
    std::size_t missing_level_mismatches = 0;
    std::size_t price_mismatches = 0;
    std::size_t quantity_mismatches = 0;
    std::optional<LobsterValidationMismatch> first_mismatch;
    LobsterSummary replay;
};

LobsterValidationSummary validate_lobster_replay(
    std::istream& message_input,
    std::istream& orderbook_input,
    OrderBook& book,
    std::size_t depth
);

}  // namespace lob
