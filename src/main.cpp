#include "MarketDataReplay.hpp"

#include <fstream>
#include <iostream>

int main() {
    std::ifstream input("data/sample_orders.csv");

    if (!input) {
        std::cerr << "Could not open data/sample_orders.csv\n";
        return 1;
    }

    lob::OrderBook book;
    auto summary = lob::replay_market_data(input, book);

    std::cout << "Events processed: " << summary.events_processed << "\n";
    std::cout << "ADD events: " << summary.add_events << "\n";
    std::cout << "CANCEL events: " << summary.cancel_events << "\n";
    std::cout << "Successful cancels: " << summary.successful_cancels << "\n";
    std::cout << "Trades generated: " << summary.trades_generated << "\n";
    std::cout << "Total traded quantity: " << summary.total_traded_quantity << "\n";
    std::cout << "Order count: " << book.order_count() << "\n";

    if (auto bid = book.best_bid()) {
        std::cout << "Best bid: " << *bid << "\n";
    } else {
        std::cout << "Best bid: empty\n";
    }

    if (auto ask = book.best_ask()) {
        std::cout << "Best ask: " << *ask << "\n";
    } else {
        std::cout << "Best ask: empty\n";
    }

    return 0;
}
