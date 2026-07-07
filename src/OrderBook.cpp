#include "OrderBook.hpp"

#include <algorithm>

namespace lob {

std::size_t OrderBook::allocate_slot(const Order& order) {
    if (!free_slots_.empty()) {
        std::size_t index = free_slots_.back();
        free_slots_.pop_back();
        pool_[index] = {order, kInvalid, kInvalid};
        return index;
    }

    pool_.push_back({order, kInvalid, kInvalid});
    return pool_.size() - 1;
}

void OrderBook::free_slot(std::size_t index) {
    free_slots_.push_back(index);
}

void OrderBook::append_to_level(PriceLevel& level, std::size_t index) {
    OrderNode& node = pool_[index];
    node.prev = level.tail;
    node.next = kInvalid;

    if (level.tail != kInvalid) {
        pool_[level.tail].next = index;
    } else {
        level.head = index;
    }

    level.tail = index;
    ++level.count;
}

void OrderBook::unlink_from_level(PriceLevel& level, std::size_t index) {
    OrderNode& node = pool_[index];

    if (node.prev != kInvalid) {
        pool_[node.prev].next = node.next;
    } else {
        level.head = node.next;
    }

    if (node.next != kInvalid) {
        pool_[node.next].prev = node.prev;
    } else {
        level.tail = node.prev;
    }

    node.prev = kInvalid;
    node.next = kInvalid;
    --level.count;
}

std::vector<Trade> OrderBook::add_order(const Order& order) {
    std::vector<Trade> trades;

    if (order.quantity <= 0) {
        return trades;
    }

    if (order_location_.find(order.id) != order_location_.end()) {
        return trades;
    }

    Order remaining = order;

    if (remaining.side == Side::Buy) {
        while (remaining.quantity > 0 &&
               !asks_.empty() &&
               asks_.begin()->first <= remaining.price) {
            auto level_it = asks_.begin();
            auto& level = level_it->second;
            std::size_t current = level.head;

            while (remaining.quantity > 0 && current != kInvalid) {
                Order& resting = pool_[current].order;

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
                    OrderId filled_order_id = resting.id;
                    std::size_t next = pool_[current].next;
                    unlink_from_level(level, current);
                    free_slot(current);
                    order_location_.erase(filled_order_id);
                    current = next;
                }
            }

            if (level.count == 0) {
                asks_.erase(level_it);
            }
        }

        if (remaining.quantity > 0) {
            std::size_t index = allocate_slot(remaining);
            append_to_level(bids_[remaining.price], index);
            order_location_[remaining.id] = index;
        }
    } else {
        while (remaining.quantity > 0 &&
               !bids_.empty() &&
               bids_.begin()->first >= remaining.price) {
            auto level_it = bids_.begin();
            auto& level = level_it->second;
            std::size_t current = level.head;

            while (remaining.quantity > 0 && current != kInvalid) {
                Order& resting = pool_[current].order;

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
                    OrderId filled_order_id = resting.id;
                    std::size_t next = pool_[current].next;
                    unlink_from_level(level, current);
                    free_slot(current);
                    order_location_.erase(filled_order_id);
                    current = next;
                }
            }

            if (level.count == 0) {
                bids_.erase(level_it);
            }
        }

        if (remaining.quantity > 0) {
            std::size_t index = allocate_slot(remaining);
            append_to_level(asks_[remaining.price], index);
            order_location_[remaining.id] = index;
        }
    }

    return trades;
}

bool OrderBook::cancel_order(OrderId order_id) {
    auto location_it = order_location_.find(order_id);

    if (location_it == order_location_.end()) {
        return false;
    }

    std::size_t index = location_it->second;
    const Order& order = pool_[index].order;

    if (order.side == Side::Buy) {
        auto level_it = bids_.find(order.price);

        if (level_it == bids_.end()) {
            return false;
        }

        unlink_from_level(level_it->second, index);
        free_slot(index);
        order_location_.erase(location_it);

        if (level_it->second.count == 0) {
            bids_.erase(level_it);
        }
    } else {
        auto level_it = asks_.find(order.price);

        if (level_it == asks_.end()) {
            return false;
        }

        unlink_from_level(level_it->second, index);
        free_slot(index);
        order_location_.erase(location_it);

        if (level_it->second.count == 0) {
            asks_.erase(level_it);
        }
    }

    return true;
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
    return order_location_.size();
}

}  // namespace lob
