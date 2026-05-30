#include "OrderBook.hpp"

#include <iostream>

int main() {
    lob::OrderBook book;

    book.add_order({
        1,
        lob::Side::Buy,
        10050,
        200,
        1000000000
    });

    book.add_order({
        2,
        lob::Side::Sell,
        10060,
        100,
        1000001000
    });

    auto trades = book.add_order({
        3,
        lob::Side::Buy,
        10060,
        50,
        1000002000
    });

    std::cout << "Trades generated: " << trades.size() << "\n";

    for (const auto& trade : trades) {
        std::cout << "Trade price: " << trade.price << "\n";
        std::cout << "Trade quantity: " << trade.quantity << "\n";
    }

    std::cout << "Order count: " << book.order_count() << "\n";

    if (auto bid = book.best_bid()) {
        std::cout << "Best bid: " << *bid << "\n";
    }

    if (auto ask = book.best_ask()) {
        std::cout << "Best ask: " << *ask << "\n";
    }

    bool cancelled = book.cancel_order(1);

    std::cout << "Cancelled order 1: " << std::boolalpha << cancelled << "\n";
    std::cout << "Order count after cancel: " << book.order_count() << "\n";

    return 0;
}
