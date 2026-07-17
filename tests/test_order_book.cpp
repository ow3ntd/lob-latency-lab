#include "OrderBook.hpp"
#include "TestSupport.hpp"

#include <iostream>
#include <vector>

void test_add_non_crossing_orders() {
    lob::OrderBook book;

    auto trades1 = book.add_order({1, lob::Side::Buy, 10050, 200, 1000});
    auto trades2 = book.add_order({2, lob::Side::Sell, 10060, 100, 1001});

    CHECK(trades1.empty());
    CHECK(trades2.empty());
    CHECK(book.order_count() == 2);
    CHECK(book.best_bid().has_value());
    CHECK(book.best_ask().has_value());
    CHECK(*book.best_bid() == 10050);
    CHECK(*book.best_ask() == 10060);
}

void test_cancel_existing_order() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 200, 1000});

    bool cancelled = book.cancel_order(1);

    CHECK(cancelled);
    CHECK(book.order_count() == 0);
    CHECK(!book.best_bid().has_value());
}

void test_cancel_missing_order() {
    lob::OrderBook book;

    bool cancelled = book.cancel_order(999);

    CHECK(!cancelled);
    CHECK(book.order_count() == 0);
    CHECK(!book.best_bid().has_value());
    CHECK(!book.best_ask().has_value());
}

void test_cancel_missing_order_does_not_change_book() {
    lob::OrderBook book;

    auto trades = book.add_order({1, lob::Side::Buy, 10050, 200, 1000});
    bool cancelled = book.cancel_order(999);

    CHECK(trades.empty());
    CHECK(!cancelled);
    CHECK(book.order_count() == 1);
    CHECK(book.best_bid().has_value());
    CHECK(*book.best_bid() == 10050);
    CHECK(!book.best_ask().has_value());
}

void test_cancel_already_filled_order() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Sell, 10060, 100, 1000});

    auto fill_trades = book.add_order({2, lob::Side::Buy, 10060, 100, 1001});
    bool cancelled = book.cancel_order(1);

    CHECK(fill_trades.size() == 1);
    CHECK(fill_trades[0].resting_order_id == 1);
    CHECK(fill_trades[0].quantity == 100);
    CHECK(!cancelled);
    CHECK(book.order_count() == 0);
    CHECK(!book.best_bid().has_value());
    CHECK(!book.best_ask().has_value());
}

void test_empty_book_best_prices() {
    lob::OrderBook book;

    CHECK(!book.best_bid().has_value());
    CHECK(!book.best_ask().has_value());
    CHECK(book.order_count() == 0);
}

void test_order_rests_when_opposite_side_empty() {
    lob::OrderBook book;

    auto trades = book.add_order({1, lob::Side::Buy, 10050, 200, 1000});

    CHECK(trades.empty());
    CHECK(book.order_count() == 1);
    CHECK(book.best_bid().has_value());
    CHECK(*book.best_bid() == 10050);
    CHECK(!book.best_ask().has_value());
}

void test_crossing_buy_partial_fill() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Sell, 10060, 100, 1000});

    auto trades = book.add_order({2, lob::Side::Buy, 10060, 50, 1001});

    CHECK(trades.size() == 1);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].aggressive_order_id == 2);
    CHECK(trades[0].price == 10060);
    CHECK(trades[0].quantity == 50);

    CHECK(book.order_count() == 1);
    CHECK(book.best_ask().has_value());
    CHECK(*book.best_ask() == 10060);
}

void test_crossing_buy_full_fill_with_remainder() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Sell, 10060, 100, 1000});

    auto trades = book.add_order({2, lob::Side::Buy, 10060, 150, 1001});

    CHECK(trades.size() == 1);
    CHECK(trades[0].price == 10060);
    CHECK(trades[0].quantity == 100);

    CHECK(book.order_count() == 1);
    CHECK(book.best_bid().has_value());
    CHECK(*book.best_bid() == 10060);
    CHECK(!book.best_ask().has_value());
}

void test_crossing_sell_partial_fill() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 200, 1000});

    auto trades = book.add_order({2, lob::Side::Sell, 10050, 75, 1001});

    CHECK(trades.size() == 1);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].aggressive_order_id == 2);
    CHECK(trades[0].price == 10050);
    CHECK(trades[0].quantity == 75);

    CHECK(book.order_count() == 1);
    CHECK(book.best_bid().has_value());
    CHECK(*book.best_bid() == 10050);
}

void test_partial_fill_keeps_positive_resting_quantity() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Sell, 10060, 100, 1000});

    auto partial_trades = book.add_order({2, lob::Side::Buy, 10060, 40, 1001});
    auto remaining_trades = book.add_order({3, lob::Side::Buy, 10060, 60, 1002});

    CHECK(partial_trades.size() == 1);
    CHECK(partial_trades[0].quantity == 40);
    CHECK(remaining_trades.size() == 1);
    CHECK(remaining_trades[0].resting_order_id == 1);
    CHECK(remaining_trades[0].quantity == 60);
    CHECK(book.order_count() == 0);
    CHECK(!book.best_ask().has_value());
}

void test_full_fill_leaves_no_negative_resting_quantity() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 75, 1000});

    auto fill_trades = book.add_order({2, lob::Side::Sell, 10050, 75, 1001});

    CHECK(fill_trades.size() == 1);
    CHECK(fill_trades[0].quantity == 75);
    CHECK(book.order_count() == 0);
    CHECK(!book.best_bid().has_value());

    auto later_trades = book.add_order({3, lob::Side::Sell, 10050, 1, 1002});

    CHECK(later_trades.empty());
    CHECK(book.order_count() == 1);
    CHECK(book.best_ask().has_value());
    CHECK(*book.best_ask() == 10050);
}

void test_basic_volume_conservation_with_remainder() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Sell, 10060, 100, 1000});

    auto trades = book.add_order({2, lob::Side::Buy, 10060, 150, 1001});
    auto remainder_trades = book.add_order({3, lob::Side::Sell, 10060, 50, 1002});

    CHECK(trades.size() == 1);
    CHECK(trades[0].quantity == 100);
    CHECK(remainder_trades.size() == 1);
    CHECK(remainder_trades[0].resting_order_id == 2);
    CHECK(remainder_trades[0].quantity == 50);
    CHECK(trades[0].quantity + remainder_trades[0].quantity == 150);
    CHECK(book.order_count() == 0);
}

void test_cancel_middle_order_preserves_fifo() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10050, 10, 1001});
    book.add_order({3, lob::Side::Buy, 10050, 10, 1002});

    bool cancelled = book.cancel_order(2);

    CHECK(cancelled);
    CHECK(book.order_count() == 2);

    auto trades = book.add_order({100, lob::Side::Sell, 10050, 15, 1003});

    CHECK(trades.size() == 2);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].quantity == 10);
    CHECK(trades[1].resting_order_id == 3);
    CHECK(trades[1].quantity == 5);
}

void test_cancel_head_order_advances_correctly() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10050, 10, 1001});

    bool cancelled = book.cancel_order(1);

    CHECK(cancelled);

    auto trades = book.add_order({100, lob::Side::Sell, 10050, 5, 1002});

    CHECK(trades.size() == 1);
    CHECK(trades[0].resting_order_id == 2);
}

void test_cancel_tail_order() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10050, 10, 1001});

    bool cancelled = book.cancel_order(2);

    CHECK(cancelled);
    CHECK(book.best_bid().has_value());
    CHECK(*book.best_bid() == 10050);
    CHECK(book.order_count() == 1);
}

void test_cancel_only_order_removes_price_level() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});

    bool cancelled = book.cancel_order(1);

    CHECK(cancelled);
    CHECK(!book.best_bid().has_value());
    CHECK(book.order_count() == 0);
}

void test_slot_reuse_after_heavy_cancel_churn() {
    lob::OrderBook book;

    for (int i = 0; i < 1000; ++i) {
        book.add_order({static_cast<lob::OrderId>(i), lob::Side::Buy, 10050, 10, static_cast<lob::Timestamp>(i)});
        book.cancel_order(static_cast<lob::OrderId>(i));
    }

    CHECK(book.order_count() == 0);
    CHECK(!book.best_bid().has_value());

    book.add_order({9001, lob::Side::Buy, 10050, 5, 1000});
    book.add_order({9002, lob::Side::Buy, 10050, 5, 1001});

    auto trades = book.add_order({9003, lob::Side::Sell, 10050, 10, 1002});

    CHECK(trades.size() == 2);
    CHECK(trades[0].resting_order_id == 9001);
    CHECK(trades[1].resting_order_id == 9002);
}

void test_reduce_rejects_invalid_quantity() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});

    CHECK(book.reduce_order(1, 0) == lob::ReduceResult::InvalidQuantity);
    CHECK(book.reduce_order(1, -1) == lob::ReduceResult::InvalidQuantity);
    CHECK(book.order_count() == 1);
}

void test_reduce_missing_order() {
    lob::OrderBook book;

    CHECK(book.reduce_order(999, 5) == lob::ReduceResult::NotFound);
    CHECK(book.order_count() == 0);
}

void test_reduce_rejects_quantity_exceeding_resting_quantity() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});

    CHECK(book.reduce_order(1, 11) == lob::ReduceResult::ExceedsQuantity);

    auto trades = book.add_order({100, lob::Side::Sell, 10050, 10, 1001});

    CHECK(trades.size() == 1);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].quantity == 10);
}

void test_partial_reduction_subtracts_quantity() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});

    CHECK(book.reduce_order(1, 4) == lob::ReduceResult::Reduced);
    CHECK(book.order_count() == 1);

    auto trades = book.add_order({100, lob::Side::Sell, 10050, 6, 1001});

    CHECK(trades.size() == 1);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].quantity == 6);
}

void test_partial_reduction_preserves_fifo_position() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10050, 10, 1001});

    CHECK(book.reduce_order(1, 4) == lob::ReduceResult::Reduced);

    auto trades = book.add_order({100, lob::Side::Sell, 10050, 7, 1002});

    CHECK(trades.size() == 2);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].quantity == 6);
    CHECK(trades[1].resting_order_id == 2);
    CHECK(trades[1].quantity == 1);
}

void test_exact_reduction_removes_order() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});

    CHECK(book.reduce_order(1, 10) == lob::ReduceResult::Removed);
    CHECK(book.order_count() == 0);
    CHECK(!book.best_bid().has_value());
    CHECK(book.reduce_order(1, 1) == lob::ReduceResult::NotFound);
}

void test_empty_book_snapshot_is_empty() {
    const lob::OrderBook book;

    auto snapshot = book.snapshot(5);

    CHECK(snapshot.bids.empty());
    CHECK(snapshot.asks.empty());
}

void test_snapshot_depth_zero_is_empty() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Sell, 10060, 20, 1001});

    auto snapshot = book.snapshot(0);

    CHECK(snapshot.bids.empty());
    CHECK(snapshot.asks.empty());
    CHECK(book.order_count() == 2);
}

void test_snapshot_aggregates_orders_at_same_price() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10050, 15, 1001});

    auto snapshot = book.snapshot(1);

    CHECK(snapshot.bids.size() == 1);
    CHECK(snapshot.bids[0].price == 10050);
    CHECK(snapshot.bids[0].quantity == 25);
    CHECK(snapshot.asks.empty());
}

void test_snapshot_orders_bids_descending() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10070, 20, 1001});
    book.add_order({3, lob::Side::Buy, 10060, 30, 1002});

    auto snapshot = book.snapshot(3);

    CHECK(snapshot.bids.size() == 3);
    CHECK(snapshot.bids[0].price == 10070);
    CHECK(snapshot.bids[1].price == 10060);
    CHECK(snapshot.bids[2].price == 10050);
}

void test_snapshot_orders_asks_ascending() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Sell, 10070, 10, 1000});
    book.add_order({2, lob::Side::Sell, 10050, 20, 1001});
    book.add_order({3, lob::Side::Sell, 10060, 30, 1002});

    auto snapshot = book.snapshot(3);

    CHECK(snapshot.asks.size() == 3);
    CHECK(snapshot.asks[0].price == 10050);
    CHECK(snapshot.asks[1].price == 10060);
    CHECK(snapshot.asks[2].price == 10070);
}

void test_snapshot_depth_truncates_each_side_independently() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10030, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10020, 20, 1001});
    book.add_order({3, lob::Side::Buy, 10010, 30, 1002});
    book.add_order({4, lob::Side::Sell, 10040, 40, 1003});
    book.add_order({5, lob::Side::Sell, 10050, 50, 1004});
    book.add_order({6, lob::Side::Sell, 10060, 60, 1005});

    auto snapshot = book.snapshot(2);

    CHECK(snapshot.bids.size() == 2);
    CHECK(snapshot.bids[0].price == 10030);
    CHECK(snapshot.bids[1].price == 10020);
    CHECK(snapshot.asks.size() == 2);
    CHECK(snapshot.asks[0].price == 10040);
    CHECK(snapshot.asks[1].price == 10050);
}

void test_snapshot_reflects_partial_reduction() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10050, 15, 1001});

    CHECK(book.reduce_order(1, 4) == lob::ReduceResult::Reduced);

    auto snapshot = book.snapshot(1);

    CHECK(snapshot.bids.size() == 1);
    CHECK(snapshot.bids[0].price == 10050);
    CHECK(snapshot.bids[0].quantity == 21);
}

void test_snapshot_omits_level_after_final_order_cancelled() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10040, 20, 1001});

    CHECK(book.cancel_order(1));

    auto snapshot = book.snapshot(2);

    CHECK(snapshot.bids.size() == 1);
    CHECK(snapshot.bids[0].price == 10040);
    CHECK(snapshot.bids[0].quantity == 20);
    CHECK(snapshot.asks.empty());
}

int main() {
    test_add_non_crossing_orders();
    test_cancel_existing_order();
    test_cancel_missing_order();
    test_cancel_missing_order_does_not_change_book();
    test_cancel_already_filled_order();
    test_empty_book_best_prices();
    test_order_rests_when_opposite_side_empty();
    test_crossing_buy_partial_fill();
    test_crossing_buy_full_fill_with_remainder();
    test_crossing_sell_partial_fill();
    test_partial_fill_keeps_positive_resting_quantity();
    test_full_fill_leaves_no_negative_resting_quantity();
    test_basic_volume_conservation_with_remainder();
    test_cancel_middle_order_preserves_fifo();
    test_cancel_head_order_advances_correctly();
    test_cancel_tail_order();
    test_cancel_only_order_removes_price_level();
    test_slot_reuse_after_heavy_cancel_churn();
    test_reduce_rejects_invalid_quantity();
    test_reduce_missing_order();
    test_reduce_rejects_quantity_exceeding_resting_quantity();
    test_partial_reduction_subtracts_quantity();
    test_partial_reduction_preserves_fifo_position();
    test_exact_reduction_removes_order();
    test_empty_book_snapshot_is_empty();
    test_snapshot_depth_zero_is_empty();
    test_snapshot_aggregates_orders_at_same_price();
    test_snapshot_orders_bids_descending();
    test_snapshot_orders_asks_ascending();
    test_snapshot_depth_truncates_each_side_independently();
    test_snapshot_reflects_partial_reduction();
    test_snapshot_omits_level_after_final_order_cancelled();

    std::cout << "All order book tests passed.\n";

    return 0;
}
