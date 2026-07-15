#pragma once

#include "Order.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace lob {

enum class ReduceResult {
    Reduced,
    Removed,
    NotFound,
    InvalidQuantity,
    ExceedsQuantity
};

class OrderBook {
public:
    OrderBook() = default;

    std::vector<Trade> add_order(const Order& order);

    bool cancel_order(OrderId order_id);

    ReduceResult reduce_order(OrderId order_id, Quantity quantity);

    std::optional<Price> best_bid() const;

    std::optional<Price> best_ask() const;

    std::size_t order_count() const;

private:
    static constexpr std::size_t kInvalid = static_cast<std::size_t>(-1);

    struct OrderNode {
        Order order;
        std::size_t prev = kInvalid;
        std::size_t next = kInvalid;
    };

    struct PriceLevel {
        std::size_t head = kInvalid;
        std::size_t tail = kInvalid;
        std::size_t count = 0;
    };

    std::size_t allocate_slot(const Order& order);
    void free_slot(std::size_t index);
    void append_to_level(PriceLevel& level, std::size_t index);
    void unlink_from_level(PriceLevel& level, std::size_t index);

    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    std::map<Price, PriceLevel> asks_;

    std::vector<OrderNode> pool_;
    std::vector<std::size_t> free_slots_;

    std::unordered_map<OrderId, std::size_t> order_location_;
};

}  // namespace lob
