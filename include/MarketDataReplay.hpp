#pragma once

#include "OrderBook.hpp"

#include <cstddef>
#include <istream>

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
// 4=visible execution, 5=hidden execution, 7=trading halt/auxiliary,
// and Direction is 1=buy (bid) side, -1=sell (ask) side.
//
// This reader replays the order-flow events (submissions, cancellations,
// deletions) through the book. LOBSTER executions are already resolved in
// the source data, so type-4 events are counted but not re-matched, and
// hidden (type 5) and halt (type 7) events are skipped. The goal is to
// exercise the engine on real NASDAQ event sequences and data shapes, not
// to reproduce NASDAQ's exact trade prints.
struct LobsterSummary {
    std::size_t events_processed = 0;   // total rows read
    std::size_t new_orders = 0;         // type 1
    std::size_t partial_cancels = 0;    // type 2
    std::size_t deletions = 0;          // type 3
    std::size_t executions = 0;         // type 4 (visible), counted not re-matched
    std::size_t hidden_executions = 0;  // type 5, skipped
    std::size_t auxiliary = 0;          // type 7 (halts etc.), skipped
    std::size_t successful_cancels = 0; // deletions/partial-cancels the book actually removed
    Quantity executed_quantity = 0;     // sum of type-4 sizes
};

LobsterSummary replay_lobster_data(std::istream& input, OrderBook& book);

}  // namespace lob
