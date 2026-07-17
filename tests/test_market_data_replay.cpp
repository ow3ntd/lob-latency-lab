#include "MarketDataReplay.hpp"
#include "TestSupport.hpp"

#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>

void check_lobster_book_parse_fails(
    std::string_view line,
    std::size_t depth,
    std::string_view expected_message
) {
    bool threw = false;

    try {
        static_cast<void>(lob::parse_lobster_book_row(line, depth));
    } catch (const std::runtime_error& error) {
        threw = true;
        CHECK(
            std::string_view(error.what()).find(expected_message) !=
            std::string_view::npos
        );
    }

    CHECK(threw);
}

void test_replay_market_data_summary_and_book_state() {
    std::istringstream input(
        "timestamp_ns,event_type,order_id,side,price,quantity\n"
        "1000000000,ADD,1,BUY,10050,200\n"
        "1000001000,ADD,2,SELL,10060,100\n"
        "1000002000,ADD,3,BUY,10060,50\n"
        "1000003000,CANCEL,1,BUY,10050,200\n"
    );

    lob::OrderBook book;
    auto summary = lob::replay_market_data(input, book);

    CHECK(summary.events_processed == 4);
    CHECK(summary.add_events == 3);
    CHECK(summary.cancel_events == 1);
    CHECK(summary.successful_cancels == 1);
    CHECK(summary.trades_generated == 1);
    CHECK(summary.total_traded_quantity == 50);
    CHECK(book.order_count() == 1);
    CHECK(book.best_ask().has_value());
    CHECK(*book.best_ask() == 10060);
    CHECK(!book.best_bid().has_value());
}

void test_lobster_partial_cancel_subtracts_event_quantity() {
    std::istringstream input(
        "34200.000000001,1,1,10,10050,1\n"
        "34200.000000002,2,1,4,10050,1\n"
    );

    lob::OrderBook book;
    auto summary = lob::replay_lobster_data(input, book);

    CHECK(summary.events_processed == 2);
    CHECK(summary.partial_cancels == 1);
    CHECK(summary.successful_cancels == 1);
    CHECK(book.order_count() == 1);

    auto trades = book.add_order({100, lob::Side::Sell, 10050, 6, 3});

    CHECK(trades.size() == 1);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].quantity == 6);
    CHECK(book.order_count() == 0);
}

void test_lobster_partial_cancel_preserves_fifo_position() {
    std::istringstream input(
        "34200.000000001,1,1,10,10050,1\n"
        "34200.000000002,1,2,10,10050,1\n"
        "34200.000000003,2,1,4,10050,1\n"
    );

    lob::OrderBook book;
    auto summary = lob::replay_lobster_data(input, book);

    CHECK(summary.partial_cancels == 1);
    CHECK(summary.successful_cancels == 1);

    auto trades = book.add_order({100, lob::Side::Sell, 10050, 7, 4});

    CHECK(trades.size() == 2);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].quantity == 6);
    CHECK(trades[1].resting_order_id == 2);
    CHECK(trades[1].quantity == 1);
}

void test_lobster_visible_execution_reduces_referenced_order() {
    std::istringstream input(
        "34200.000000001,1,1,10,10060,-1\n"
        "34200.000000002,4,1,4,10060,-1\n"
    );

    lob::OrderBook book;
    auto summary = lob::replay_lobster_data(input, book);

    CHECK(summary.executions == 1);
    CHECK(summary.successful_execution_reductions == 1);
    CHECK(summary.executed_quantity == 4);
    CHECK(book.order_count() == 1);

    auto trades = book.add_order({100, lob::Side::Buy, 10060, 6, 3});

    CHECK(trades.size() == 1);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].quantity == 6);
}

void test_lobster_visible_execution_exhausts_order() {
    std::istringstream input(
        "34200.000000001,1,1,10,10060,-1\n"
        "34200.000000002,4,1,10,10060,-1\n"
    );

    lob::OrderBook book;
    auto summary = lob::replay_lobster_data(input, book);

    CHECK(summary.executions == 1);
    CHECK(summary.successful_execution_reductions == 1);
    CHECK(summary.executed_quantity == 10);
    CHECK(book.order_count() == 0);
    CHECK(!book.best_ask().has_value());
}

void test_lobster_cross_trade_does_not_mutate_visible_book() {
    std::istringstream input(
        "34200.000000001,1,1,10,10050,1\n"
        "34200.000000002,6,0,25,10055,0\n"
    );

    lob::OrderBook book;
    auto summary = lob::replay_lobster_data(input, book);

    CHECK(summary.events_processed == 2);
    CHECK(summary.cross_trades == 1);
    CHECK(book.order_count() == 1);
    CHECK(book.best_bid().has_value());
    CHECK(*book.best_bid() == 10050);

    auto trades = book.add_order({100, lob::Side::Sell, 10050, 10, 3});

    CHECK(trades.size() == 1);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].quantity == 10);
}

void test_lobster_unsuccessful_reductions_are_not_counted() {
    std::istringstream input(
        "34200.000000001,1,1,10,10050,1\n"
        "34200.000000002,2,999,1,10050,1\n"
        "34200.000000003,4,1,11,10050,1\n"
    );

    lob::OrderBook book;
    auto summary = lob::replay_lobster_data(input, book);

    CHECK(summary.events_processed == 3);
    CHECK(summary.partial_cancels == 1);
    CHECK(summary.successful_cancels == 0);
    CHECK(summary.executions == 1);
    CHECK(summary.successful_execution_reductions == 0);
    CHECK(summary.executed_quantity == 11);
    CHECK(book.order_count() == 1);

    auto trades = book.add_order({100, lob::Side::Sell, 10050, 10, 4});

    CHECK(trades.size() == 1);
    CHECK(trades[0].resting_order_id == 1);
    CHECK(trades[0].quantity == 10);
    CHECK(book.order_count() == 0);
}

void test_parse_lobster_book_row_one_valid_level() {
    auto row = lob::parse_lobster_book_row("10100,25,10090,30", 1);

    CHECK(row.asks.size() == 1);
    CHECK(row.asks[0].price == 10100);
    CHECK(row.asks[0].quantity == 25);
    CHECK(row.bids.size() == 1);
    CHECK(row.bids[0].price == 10090);
    CHECK(row.bids[0].quantity == 30);
}

void test_parse_lobster_book_row_preserves_level_order() {
    auto row = lob::parse_lobster_book_row(
        "10100,10,10090,20,"
        "10110,30,10080,40,"
        "10120,50,10070,60",
        3
    );

    CHECK(row.asks.size() == 3);
    CHECK(row.asks[0].price == 10100);
    CHECK(row.asks[0].quantity == 10);
    CHECK(row.asks[1].price == 10110);
    CHECK(row.asks[1].quantity == 30);
    CHECK(row.asks[2].price == 10120);
    CHECK(row.asks[2].quantity == 50);
    CHECK(row.bids.size() == 3);
    CHECK(row.bids[0].price == 10090);
    CHECK(row.bids[0].quantity == 20);
    CHECK(row.bids[1].price == 10080);
    CHECK(row.bids[1].quantity == 40);
    CHECK(row.bids[2].price == 10070);
    CHECK(row.bids[2].quantity == 60);
}

void test_parse_lobster_book_row_one_side_empty() {
    auto row = lob::parse_lobster_book_row("9999999999,0,10090,30", 1);

    CHECK(row.asks.empty());
    CHECK(row.bids.size() == 1);
    CHECK(row.bids[0].price == 10090);
    CHECK(row.bids[0].quantity == 30);
}

void test_parse_lobster_book_row_both_sides_empty() {
    auto row = lob::parse_lobster_book_row("9999999999,0,-9999999999,0", 1);

    CHECK(row.asks.empty());
    CHECK(row.bids.empty());
}

void test_parse_lobster_book_row_rejects_sentinel_with_quantity() {
    check_lobster_book_parse_fails(
        "9999999999,1,10090,30", 1, "level 1, ask price"
    );
    check_lobster_book_parse_fails(
        "10100,25,-9999999999,1", 1, "level 1, bid price"
    );
}

void test_parse_lobster_book_row_rejects_too_few_fields() {
    check_lobster_book_parse_fails(
        "10100,25,10090", 1, "expected 4 fields, got 3"
    );
}

void test_parse_lobster_book_row_rejects_too_many_fields() {
    check_lobster_book_parse_fails(
        "10100,25,10090,30,10110", 1, "expected 4 fields, got 5"
    );
}

void test_parse_lobster_book_row_rejects_empty_field() {
    check_lobster_book_parse_fails(
        "10100,,10090,30", 1, "level 1, ask size: empty field"
    );
}

void test_parse_lobster_book_row_rejects_malformed_integer() {
    check_lobster_book_parse_fails(
        "invalid,25,10090,30", 1, "level 1, ask price: invalid integer"
    );
}

void test_parse_lobster_book_row_rejects_negative_quantity() {
    check_lobster_book_parse_fails(
        "10100,-1,10090,30", 1, "level 1, ask size: negative quantity"
    );
}

void test_parse_lobster_book_row_rejects_invalid_price() {
    check_lobster_book_parse_fails(
        "0,25,10090,30", 1, "level 1, ask price: price must be positive"
    );
    check_lobster_book_parse_fails(
        "10100,25,-10090,30", 1, "level 1, bid price: price must be positive"
    );
}

void test_parse_lobster_book_row_zero_depth_empty_input() {
    auto row = lob::parse_lobster_book_row("", 0);

    CHECK(row.asks.empty());
    CHECK(row.bids.empty());
}

void test_parse_lobster_book_row_zero_depth_rejects_input() {
    check_lobster_book_parse_fails(
        "10100,25,10090,30", 0, "depth 0: expected empty input"
    );
}

void test_parse_lobster_book_row_rejects_zero_ask_size() {
    check_lobster_book_parse_fails(
        "10100,0,10090,30", 1,
        "level 1, ask size: occupied level requires positive quantity"
    );
}

void test_parse_lobster_book_row_rejects_zero_bid_size() {
    check_lobster_book_parse_fails(
        "10100,25,10090,0", 1,
        "level 1, bid size: occupied level requires positive quantity"
    );
}

void test_parse_lobster_book_row_rejects_ask_after_sentinel() {
    check_lobster_book_parse_fails(
        "9999999999,0,10090,30,10110,25,10080,40", 2,
        "level 2, ask price: occupied level follows empty-level sentinel"
    );
}

void test_parse_lobster_book_row_rejects_bid_after_sentinel() {
    check_lobster_book_parse_fails(
        "10100,25,-9999999999,0,10110,35,10080,40", 2,
        "level 2, bid price: occupied level follows empty-level sentinel"
    );
}

void test_parse_lobster_book_row_rejects_invalid_ask_order() {
    check_lobster_book_parse_fails(
        "10100,10,10090,20,10100,30,10080,40", 2,
        "level 2, ask price: ask prices must be strictly increasing"
    );
    check_lobster_book_parse_fails(
        "10100,10,10090,20,10090,30,10080,40", 2,
        "level 2, ask price: ask prices must be strictly increasing"
    );
}

void test_parse_lobster_book_row_rejects_invalid_bid_order() {
    check_lobster_book_parse_fails(
        "10100,10,10090,20,10110,30,10090,40", 2,
        "level 2, bid price: bid prices must be strictly decreasing"
    );
    check_lobster_book_parse_fails(
        "10100,10,10090,20,10110,30,10100,40", 2,
        "level 2, bid price: bid prices must be strictly decreasing"
    );
}

void test_parse_lobster_book_row_rejects_depth_overflow() {
    check_lobster_book_parse_fails(
        "", std::numeric_limits<std::size_t>::max(),
        "depth exceeds supported field count"
    );
}

void test_parse_lobster_book_row_rejects_bid_sentinel_on_ask_side() {
    check_lobster_book_parse_fails(
        "-9999999999,10,10090,20", 1, "wrong-side empty-level sentinel"
    );
}

void test_parse_lobster_book_row_rejects_ask_sentinel_on_bid_side() {
    check_lobster_book_parse_fails(
        "10100,10,9999999999,20", 1, "wrong-side empty-level sentinel"
    );
}

int main() {
    test_replay_market_data_summary_and_book_state();
    test_lobster_partial_cancel_subtracts_event_quantity();
    test_lobster_partial_cancel_preserves_fifo_position();
    test_lobster_visible_execution_reduces_referenced_order();
    test_lobster_visible_execution_exhausts_order();
    test_lobster_cross_trade_does_not_mutate_visible_book();
    test_lobster_unsuccessful_reductions_are_not_counted();
    test_parse_lobster_book_row_one_valid_level();
    test_parse_lobster_book_row_preserves_level_order();
    test_parse_lobster_book_row_one_side_empty();
    test_parse_lobster_book_row_both_sides_empty();
    test_parse_lobster_book_row_rejects_sentinel_with_quantity();
    test_parse_lobster_book_row_rejects_too_few_fields();
    test_parse_lobster_book_row_rejects_too_many_fields();
    test_parse_lobster_book_row_rejects_empty_field();
    test_parse_lobster_book_row_rejects_malformed_integer();
    test_parse_lobster_book_row_rejects_negative_quantity();
    test_parse_lobster_book_row_rejects_invalid_price();
    test_parse_lobster_book_row_zero_depth_empty_input();
    test_parse_lobster_book_row_zero_depth_rejects_input();
    test_parse_lobster_book_row_rejects_zero_ask_size();
    test_parse_lobster_book_row_rejects_zero_bid_size();
    test_parse_lobster_book_row_rejects_ask_after_sentinel();
    test_parse_lobster_book_row_rejects_bid_after_sentinel();
    test_parse_lobster_book_row_rejects_invalid_ask_order();
    test_parse_lobster_book_row_rejects_invalid_bid_order();
    test_parse_lobster_book_row_rejects_depth_overflow();
    test_parse_lobster_book_row_rejects_bid_sentinel_on_ask_side();
    test_parse_lobster_book_row_rejects_ask_sentinel_on_bid_side();

    std::cout << "All market data replay tests passed.\n";

    return 0;
}
