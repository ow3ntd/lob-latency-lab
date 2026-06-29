#include "MarketDataReplay.hpp"

#include <cassert>
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

    assert(summary.events_processed == 4);
    assert(summary.add_events == 3);
    assert(summary.cancel_events == 1);
    assert(summary.successful_cancels == 1);
    assert(summary.trades_generated == 1);
    assert(summary.total_traded_quantity == 50);
    assert(book.order_count() == 1);
    assert(book.best_ask().has_value());
    assert(*book.best_ask() == 10060);
    assert(!book.best_bid().has_value());
}

int main() {
    test_replay_market_data_summary_and_book_state();

    std::cout << "All market data replay tests passed.\n";

    return 0;
}

