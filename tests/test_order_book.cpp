#include "OrderBook.hpp"

#include <cassert>
#include <iostream>
#include <vector>

void test_add_non_crossing_orders() {
    lob::OrderBook book;

    auto trades1 = book.add_order({1, lob::Side::Buy, 10050, 200, 1000});
    auto trades2 = book.add_order({2, lob::Side::Sell, 10060, 100, 1001});

    assert(trades1.empty());
    assert(trades2.empty());
    assert(book.order_count() == 2);
    assert(book.best_bid().has_value());
    assert(book.best_ask().has_value());
    assert(*book.best_bid() == 10050);
    assert(*book.best_ask() == 10060);
}

void test_cancel_existing_order() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 200, 1000});

    bool cancelled = book.cancel_order(1);

    assert(cancelled);
    assert(book.order_count() == 0);
    assert(!book.best_bid().has_value());
}

void test_cancel_missing_order() {
    lob::OrderBook book;

    bool cancelled = book.cancel_order(999);

    assert(!cancelled);
    assert(book.order_count() == 0);
    assert(!book.best_bid().has_value());
    assert(!book.best_ask().has_value());
}

void test_cancel_missing_order_does_not_change_book() {
    lob::OrderBook book;

    auto trades = book.add_order({1, lob::Side::Buy, 10050, 200, 1000});
    bool cancelled = book.cancel_order(999);

    assert(trades.empty());
    assert(!cancelled);
    assert(book.order_count() == 1);
    assert(book.best_bid().has_value());
    assert(*book.best_bid() == 10050);
    assert(!book.best_ask().has_value());
}

void test_cancel_already_filled_order() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Sell, 10060, 100, 1000});

    auto fill_trades = book.add_order({2, lob::Side::Buy, 10060, 100, 1001});
    bool cancelled = book.cancel_order(1);

    assert(fill_trades.size() == 1);
    assert(fill_trades[0].resting_order_id == 1);
    assert(fill_trades[0].quantity == 100);
    assert(!cancelled);
    assert(book.order_count() == 0);
    assert(!book.best_bid().has_value());
    assert(!book.best_ask().has_value());
}

void test_empty_book_best_prices() {
    lob::OrderBook book;

    assert(!book.best_bid().has_value());
    assert(!book.best_ask().has_value());
    assert(book.order_count() == 0);
}

void test_order_rests_when_opposite_side_empty() {
    lob::OrderBook book;

    auto trades = book.add_order({1, lob::Side::Buy, 10050, 200, 1000});

    assert(trades.empty());
    assert(book.order_count() == 1);
    assert(book.best_bid().has_value());
    assert(*book.best_bid() == 10050);
    assert(!book.best_ask().has_value());
}

void test_crossing_buy_partial_fill() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Sell, 10060, 100, 1000});

    auto trades = book.add_order({2, lob::Side::Buy, 10060, 50, 1001});

    assert(trades.size() == 1);
    assert(trades[0].resting_order_id == 1);
    assert(trades[0].aggressive_order_id == 2);
    assert(trades[0].price == 10060);
    assert(trades[0].quantity == 50);

    assert(book.order_count() == 1);
    assert(book.best_ask().has_value());
    assert(*book.best_ask() == 10060);
}

void test_crossing_buy_full_fill_with_remainder() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Sell, 10060, 100, 1000});

    auto trades = book.add_order({2, lob::Side::Buy, 10060, 150, 1001});

    assert(trades.size() == 1);
    assert(trades[0].price == 10060);
    assert(trades[0].quantity == 100);

    assert(book.order_count() == 1);
    assert(book.best_bid().has_value());
    assert(*book.best_bid() == 10060);
    assert(!book.best_ask().has_value());
}

void test_crossing_sell_partial_fill() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 200, 1000});

    auto trades = book.add_order({2, lob::Side::Sell, 10050, 75, 1001});

    assert(trades.size() == 1);
    assert(trades[0].resting_order_id == 1);
    assert(trades[0].aggressive_order_id == 2);
    assert(trades[0].price == 10050);
    assert(trades[0].quantity == 75);

    assert(book.order_count() == 1);
    assert(book.best_bid().has_value());
    assert(*book.best_bid() == 10050);
}

void test_partial_fill_keeps_positive_resting_quantity() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Sell, 10060, 100, 1000});

    auto partial_trades = book.add_order({2, lob::Side::Buy, 10060, 40, 1001});
    auto remaining_trades = book.add_order({3, lob::Side::Buy, 10060, 60, 1002});

    assert(partial_trades.size() == 1);
    assert(partial_trades[0].quantity == 40);
    assert(remaining_trades.size() == 1);
    assert(remaining_trades[0].resting_order_id == 1);
    assert(remaining_trades[0].quantity == 60);
    assert(book.order_count() == 0);
    assert(!book.best_ask().has_value());
}

void test_full_fill_leaves_no_negative_resting_quantity() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 75, 1000});

    auto fill_trades = book.add_order({2, lob::Side::Sell, 10050, 75, 1001});

    assert(fill_trades.size() == 1);
    assert(fill_trades[0].quantity == 75);
    assert(book.order_count() == 0);
    assert(!book.best_bid().has_value());

    auto later_trades = book.add_order({3, lob::Side::Sell, 10050, 1, 1002});

    assert(later_trades.empty());
    assert(book.order_count() == 1);
    assert(book.best_ask().has_value());
    assert(*book.best_ask() == 10050);
}

void test_basic_volume_conservation_with_remainder() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Sell, 10060, 100, 1000});

    auto trades = book.add_order({2, lob::Side::Buy, 10060, 150, 1001});
    auto remainder_trades = book.add_order({3, lob::Side::Sell, 10060, 50, 1002});

    assert(trades.size() == 1);
    assert(trades[0].quantity == 100);
    assert(remainder_trades.size() == 1);
    assert(remainder_trades[0].resting_order_id == 2);
    assert(remainder_trades[0].quantity == 50);
    assert(trades[0].quantity + remainder_trades[0].quantity == 150);
    assert(book.order_count() == 0);
}

void test_cancel_middle_order_preserves_fifo() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10050, 10, 1001});
    book.add_order({3, lob::Side::Buy, 10050, 10, 1002});

    bool cancelled = book.cancel_order(2);

    assert(cancelled);
    assert(book.order_count() == 2);

    auto trades = book.add_order({100, lob::Side::Sell, 10050, 15, 1003});

    assert(trades.size() == 2);
    assert(trades[0].resting_order_id == 1);
    assert(trades[0].quantity == 10);
    assert(trades[1].resting_order_id == 3);
    assert(trades[1].quantity == 5);
}

void test_cancel_head_order_advances_correctly() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10050, 10, 1001});

    bool cancelled = book.cancel_order(1);

    assert(cancelled);

    auto trades = book.add_order({100, lob::Side::Sell, 10050, 5, 1002});

    assert(trades.size() == 1);
    assert(trades[0].resting_order_id == 2);
}

void test_cancel_tail_order() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});
    book.add_order({2, lob::Side::Buy, 10050, 10, 1001});

    bool cancelled = book.cancel_order(2);

    assert(cancelled);
    assert(book.best_bid().has_value());
    assert(*book.best_bid() == 10050);
    assert(book.order_count() == 1);
}

void test_cancel_only_order_removes_price_level() {
    lob::OrderBook book;

    book.add_order({1, lob::Side::Buy, 10050, 10, 1000});

    bool cancelled = book.cancel_order(1);

    assert(cancelled);
    assert(!book.best_bid().has_value());
    assert(book.order_count() == 0);
}

void test_slot_reuse_after_heavy_cancel_churn() {
    lob::OrderBook book;

    for (int i = 0; i < 1000; ++i) {
        book.add_order({static_cast<lob::OrderId>(i), lob::Side::Buy, 10050, 10, static_cast<lob::Timestamp>(i)});
        book.cancel_order(static_cast<lob::OrderId>(i));
    }

    assert(book.order_count() == 0);
    assert(!book.best_bid().has_value());

    book.add_order({9001, lob::Side::Buy, 10050, 5, 1000});
    book.add_order({9002, lob::Side::Buy, 10050, 5, 1001});

    auto trades = book.add_order({9003, lob::Side::Sell, 10050, 10, 1002});

    assert(trades.size() == 2);
    assert(trades[0].resting_order_id == 9001);
    assert(trades[1].resting_order_id == 9002);
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

    std::cout << "All order book tests passed.\n";

    return 0;
}