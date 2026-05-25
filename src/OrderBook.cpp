#include "OrderBook.hpp"

#include <algorithm>

namespace lob {

std::vector<Trade> OrderBook::add_order(const Order& order) {
    if (order.quantity <= 0) {
        return {};
    }

    if (order.side == Side::Buy) {
        bids_[order.price].push_back(order);
    } else {
        asks_[order.price].push_back(order);
    }

    order_side_[order.id] = order.side;

    return {};
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
