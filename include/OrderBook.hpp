#pragma once

#include "Order.hpp"

#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace lob {

class OrderBook {
public:
    OrderBook() = default;

    std::vector<Trade> add_order(const Order& order);

    bool cancel_order(OrderId order_id);

    std::optional<Price> best_bid() const;

    std::optional<Price> best_ask() const;

    std::size_t order_count() const;

private:
    using PriceLevel = std::vector<Order>;

    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    std::map<Price, PriceLevel> asks_;

    std::unordered_map<OrderId, Side> order_side_;
};

}  // namespace lob
