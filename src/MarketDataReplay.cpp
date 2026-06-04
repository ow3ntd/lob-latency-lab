#include "MarketDataReplay.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace lob {
namespace {

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream stream(line);
    std::string field;

    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }

    return fields;
}

std::runtime_error malformed_row(std::size_t line_number, const std::string& reason) {
    return std::runtime_error(
        "Malformed row on line " + std::to_string(line_number) + ": " + reason
    );
}

Timestamp parse_timestamp(const std::string& value, std::size_t line_number) {
    try {
        std::size_t parsed = 0;
        auto result = static_cast<Timestamp>(std::stoull(value, &parsed));

        if (parsed != value.size()) {
            throw malformed_row(line_number, "invalid timestamp");
        }

        return result;
    } catch (const std::invalid_argument&) {
        throw malformed_row(line_number, "invalid timestamp");
    } catch (const std::out_of_range&) {
        throw malformed_row(line_number, "timestamp out of range");
    }
}

OrderId parse_order_id(const std::string& value, std::size_t line_number) {
    try {
        std::size_t parsed = 0;
        auto result = static_cast<OrderId>(std::stoull(value, &parsed));

        if (parsed != value.size()) {
            throw malformed_row(line_number, "invalid order id");
        }

        return result;
    } catch (const std::invalid_argument&) {
        throw malformed_row(line_number, "invalid order id");
    } catch (const std::out_of_range&) {
        throw malformed_row(line_number, "order id out of range");
    }
}

Price parse_price(const std::string& value, std::size_t line_number) {
    try {
        std::size_t parsed = 0;
        auto result = static_cast<Price>(std::stoll(value, &parsed));

        if (parsed != value.size()) {
            throw malformed_row(line_number, "invalid price");
        }

        return result;
    } catch (const std::invalid_argument&) {
        throw malformed_row(line_number, "invalid price");
    } catch (const std::out_of_range&) {
        throw malformed_row(line_number, "price out of range");
    }
}

Quantity parse_quantity(const std::string& value, std::size_t line_number) {
    try {
        std::size_t parsed = 0;
        auto result = static_cast<Quantity>(std::stoll(value, &parsed));

        if (parsed != value.size()) {
            throw malformed_row(line_number, "invalid quantity");
        }

        return result;
    } catch (const std::invalid_argument&) {
        throw malformed_row(line_number, "invalid quantity");
    } catch (const std::out_of_range&) {
        throw malformed_row(line_number, "quantity out of range");
    }
}

Side parse_side(const std::string& value, std::size_t line_number) {
    if (value == "BUY") {
        return Side::Buy;
    }

    if (value == "SELL") {
        return Side::Sell;
    }

    throw malformed_row(line_number, "invalid side");
}

}  // namespace

ReplaySummary replay_market_data(std::istream& input, OrderBook& book) {
    ReplaySummary summary;

    std::string line;
    std::size_t line_number = 0;

    if (std::getline(input, line)) {
        ++line_number;
    }

    while (std::getline(input, line)) {
        ++line_number;

        if (line.empty()) {
            continue;
        }

        auto fields = split_csv_line(line);

        if (fields.size() != 6) {
            throw malformed_row(line_number, "expected 6 fields");
        }

        Order order{
            parse_order_id(fields[2], line_number),
            parse_side(fields[3], line_number),
            parse_price(fields[4], line_number),
            parse_quantity(fields[5], line_number),
            parse_timestamp(fields[0], line_number)
        };

        const std::string& event_type = fields[1];
        ++summary.events_processed;

        if (event_type == "ADD") {
            auto trades = book.add_order(order);

            ++summary.add_events;
            summary.trades_generated += trades.size();

            for (const auto& trade : trades) {
                summary.total_traded_quantity += trade.quantity;
            }
        } else if (event_type == "CANCEL") {
            ++summary.cancel_events;

            if (book.cancel_order(order.id)) {
                ++summary.successful_cancels;
            }
        } else {
            throw malformed_row(line_number, "invalid event type");
        }
    }

    return summary;
}

}  // namespace lob
