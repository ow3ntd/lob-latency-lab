#include "MarketDataReplay.hpp"
#include "TestSupport.hpp"

#include <iostream>
#include <sstream>

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

int main() {
    test_replay_market_data_summary_and_book_state();
    test_lobster_partial_cancel_subtracts_event_quantity();
    test_lobster_partial_cancel_preserves_fifo_position();
    test_lobster_visible_execution_reduces_referenced_order();
    test_lobster_visible_execution_exhausts_order();
    test_lobster_cross_trade_does_not_mutate_visible_book();
    test_lobster_unsuccessful_reductions_are_not_counted();

    std::cout << "All market data replay tests passed.\n";

    return 0;
}
