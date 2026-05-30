#include "OrderBook.hpp"

#include <algorithm>

namespace lob {

std::vector<Trade> OrderBook::add_order(const Order& order) {
    std::vector<Trade> trades;

    if (order.quantity <= 0) {
        return trades;
    }

    if (order_side_.find(order.id) != order_side_.end()) {
        return trades;
    }

    Order remaining = order;

    if (remaining.side == Side::Buy) {
        while (remaining.quantity > 0 &&
               !asks_.empty() &&
               asks_.begin()->first <= remaining.price) {
            auto level_it = asks_.begin();
            auto& resting_orders = level_it->second;

            while (remaining.quantity > 0 && !resting_orders.empty()) {
                Order& resting = resting_orders.front();

                Quantity trade_quantity = std::min(remaining.quantity, resting.quantity);

                trades.push_back({
                    resting.id,
                    remaining.id,
                    resting.price,
                    trade_quantity,
                    remaining.timestamp
                });

                remaining.quantity -= trade_quantity;
                resting.quantity -= trade_quantity;

                if (resting.quantity == 0) {
                    order_side_.erase(resting.id);
                    resting_orders.erase(resting_orders.begin());
                }
            }

            if (resting_orders.empty()) {
                asks_.erase(level_it);
            }
        }

        if (remaining.quantity > 0) {
            bids_[remaining.price].push_back(remaining);
            order_side_[remaining.id] = remaining.side;
        }
    } else {
        while (remaining.quantity > 0 &&
               !bids_.empty() &&
               bids_.begin()->first >= remaining.price) {
            auto level_it = bids_.begin();
            auto& resting_orders = level_it->second;

            while (remaining.quantity > 0 && !resting_orders.empty()) {
                Order& resting = resting_orders.front();

                Quantity trade_quantity = std::min(remaining.quantity, resting.quantity);

                trades.push_back({
                    resting.id,
                    remaining.id,
                    resting.price,
                    trade_quantity,
                    remaining.timestamp
                });

                remaining.quantity -= trade_quantity;
                resting.quantity -= trade_quantity;

                if (resting.quantity == 0) {
                    order_side_.erase(resting.id);
                    resting_orders.erase(resting_orders.begin());
                }
            }

            if (resting_orders.empty()) {
                bids_.erase(level_it);
            }
        }

        if (remaining.quantity > 0) {
            asks_[remaining.price].push_back(remaining);
            order_side_[remaining.id] = remaining.side;
        }
    }

    return trades;
}

bool OrderBook::cancel_order(OrderId order_id) {
    auto side_it = order_side_.find(order_id);

    if (side_it == order_side_.end()) {
        return false;
    }

    auto erase_from_book = [order_id](auto& book) {
        for (auto level_it = book.begin(); level_it != book.end(); ++level_it) {
            auto& orders = level_it->second;

            auto order_it = std::find_if(
                orders.begin(),
                orders.end(),
                [order_id](const Order& order) {
                    return order.id == order_id;
                }
            );

            if (order_it != orders.end()) {
                orders.erase(order_it);

                if (orders.empty()) {
                    book.erase(level_it);
                }

                return true;
            }
        }

        return false;
    };

    bool removed = false;

    if (side_it->second == Side::Buy) {
        removed = erase_from_book(bids_);
    } else {
        removed = erase_from_book(asks_);
    }

    if (removed) {
        order_side_.erase(side_it);
    }

    return removed;
}

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }

    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }

    return asks_.begin()->first;
}

std::size_t OrderBook::order_count() const {
    return order_side_.size();
}

}  // namespace lob
