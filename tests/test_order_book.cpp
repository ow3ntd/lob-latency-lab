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

int main() {
    test_add_non_crossing_orders();
    test_cancel_existing_order();
    test_cancel_missing_order();
    test_crossing_buy_partial_fill();
    test_crossing_buy_full_fill_with_remainder();
    test_crossing_sell_partial_fill();

    std::cout << "All order book tests passed.\n";

    return 0;
}
