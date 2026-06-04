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

}  // namespace lob
