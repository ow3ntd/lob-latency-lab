#include "MarketDataReplay.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
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

std::vector<std::string_view> split_csv_fields(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;

    while (true) {
        std::size_t comma = line.find(',', start);

        if (comma == std::string_view::npos) {
            fields.push_back(line.substr(start));
            break;
        }

        fields.push_back(line.substr(start, comma - start));
        start = comma + 1;
    }

    return fields;
}

std::runtime_error malformed_book_field(
    std::size_t level,
    const char* field,
    const std::string& reason
) {
    return std::runtime_error(
        "Malformed LOBSTER book row at level " + std::to_string(level) +
        ", " + field + ": " + reason
    );
}

std::int64_t parse_book_integer(
    std::string_view value,
    std::size_t level,
    const char* field
) {
    if (value.empty()) {
        throw malformed_book_field(level, field, "empty field");
    }

    std::int64_t result = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    auto parsed = std::from_chars(begin, end, result);

    if (parsed.ec == std::errc::result_out_of_range) {
        throw malformed_book_field(level, field, "integer out of range");
    }

    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        throw malformed_book_field(level, field, "invalid integer");
    }

    return result;
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

// LOBSTER stores the event type as a small integer code and the side as a
// direction of 1 (bid) or -1 (ask). Timestamps are seconds-after-midnight
// with a decimal part; we parse them as a fixed-point integer of nanoseconds
// so the engine keeps a monotonic integer timestamp without floating point.
long parse_lobster_int(const std::string& value, std::size_t line_number,
                       const char* what) {
    try {
        std::size_t parsed = 0;
        long result = std::stol(value, &parsed);

        if (parsed != value.size()) {
            throw malformed_row(line_number, std::string("invalid ") + what);
        }

        return result;
    } catch (const std::invalid_argument&) {
        throw malformed_row(line_number, std::string("invalid ") + what);
    } catch (const std::out_of_range&) {
        throw malformed_row(line_number, std::string(what) + " out of range");
    }
}

// Convert a LOBSTER "seconds after midnight" timestamp (which may carry a
// fractional part, e.g. "34200.017459617") into an integer nanosecond count.
// We avoid floating point by splitting on the decimal point and scaling the
// fractional digits to nanosecond resolution.
Timestamp parse_lobster_timestamp(const std::string& value, std::size_t line_number) {
    auto dot = value.find('.');

    std::string whole = (dot == std::string::npos) ? value : value.substr(0, dot);
    std::string frac  = (dot == std::string::npos) ? "" : value.substr(dot + 1);

    Timestamp seconds = parse_timestamp(whole, line_number);

    // Pad or truncate the fractional part to exactly 9 digits (nanoseconds).
    if (frac.size() > 9) {
        frac = frac.substr(0, 9);
    }
    while (frac.size() < 9) {
        frac.push_back('0');
    }

    Timestamp nanos = frac.empty() ? 0 : parse_timestamp(frac, line_number);

    return seconds * 1'000'000'000ULL + nanos;
}

Side lobster_direction_to_side(long direction, std::size_t line_number) {
    if (direction == 1) {
        return Side::Buy;
    }
    if (direction == -1) {
        return Side::Sell;
    }
    throw malformed_row(line_number, "invalid direction");
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

LobsterSummary replay_lobster_data(std::istream& input, OrderBook& book) {
    LobsterSummary summary;

    std::string line;
    std::size_t line_number = 0;

    // LOBSTER message files have no header row, so we do NOT skip the first
    // line here (unlike our own CSV format, which is headed).
    while (std::getline(input, line)) {
        ++line_number;

        if (line.empty()) {
            continue;
        }

        auto fields = split_csv_line(line);

        if (fields.size() < 6) {
            throw malformed_row(line_number, "expected at least 6 fields");
        }

        Timestamp timestamp = parse_lobster_timestamp(fields[0], line_number);
        long type = parse_lobster_int(fields[1], line_number, "type");
        OrderId order_id = parse_order_id(fields[2], line_number);
        Quantity size = parse_quantity(fields[3], line_number);
        Price price = parse_price(fields[4], line_number);
        long direction = parse_lobster_int(fields[5], line_number, "direction");

        ++summary.events_processed;

        switch (type) {
            case 1: {  // new limit order
                Order order{
                    order_id,
                    lobster_direction_to_side(direction, line_number),
                    price,
                    size,
                    timestamp
                };
                book.add_order(order);
                ++summary.new_orders;
                break;
            }
            case 2: {  // partial cancellation: reduce resting size
                ++summary.partial_cancels;

                switch (book.reduce_order(order_id, size)) {
                    case ReduceResult::Reduced:
                    case ReduceResult::Removed:
                        ++summary.successful_cancels;
                        break;
                    case ReduceResult::NotFound:
                    case ReduceResult::InvalidQuantity:
                    case ReduceResult::ExceedsQuantity:
                        break;
                }

                break;
            }
            case 3: {  // deletion (full cancel)
                ++summary.deletions;
                if (book.cancel_order(order_id)) {
                    ++summary.successful_cancels;
                }
                break;
            }
            case 4: {  // visible execution: reduce the referenced resting order
                ++summary.executions;
                summary.executed_quantity += size;

                switch (book.reduce_order(order_id, size)) {
                    case ReduceResult::Reduced:
                    case ReduceResult::Removed:
                        ++summary.successful_execution_reductions;
                        break;
                    case ReduceResult::NotFound:
                    case ReduceResult::InvalidQuantity:
                    case ReduceResult::ExceedsQuantity:
                        break;
                }

                break;
            }
            case 5: {  // hidden execution: not part of the visible book
                ++summary.hidden_executions;
                break;
            }
            case 6: {  // cross trade: not part of the visible book
                ++summary.cross_trades;
                break;
            }
            case 7: {  // trading halt / auxiliary
                ++summary.auxiliary;
                break;
            }
            default:
                throw malformed_row(line_number, "unknown LOBSTER event type");
        }
    }

    return summary;
}

LobsterBookRow parse_lobster_book_row(
    std::string_view line,
    std::size_t depth
) {
    if (depth == 0) {
        if (!line.empty()) {
            throw std::runtime_error(
                "Malformed LOBSTER book row for depth 0: expected empty input"
            );
        }

        return {};
    }

    if (depth > std::numeric_limits<std::size_t>::max() / 4) {
        throw std::runtime_error(
            "Malformed LOBSTER book row: depth exceeds supported field count"
        );
    }

    std::size_t expected_fields = depth * 4;
    auto fields = split_csv_fields(line);

    if (fields.size() != expected_fields) {
        throw std::runtime_error(
            "Malformed LOBSTER book row for depth " +
            std::to_string(depth) + ": expected " +
            std::to_string(expected_fields) + " fields, got " +
            std::to_string(fields.size())
        );
    }

    constexpr Price kEmptyAskPrice = 9'999'999'999;
    constexpr Price kEmptyBidPrice = -9'999'999'999;

    LobsterBookRow row;
    row.asks.reserve(depth);
    row.bids.reserve(depth);

    bool ask_sentinel_seen = false;
    bool bid_sentinel_seen = false;
    bool have_previous_ask = false;
    bool have_previous_bid = false;
    Price previous_ask = 0;
    Price previous_bid = 0;

    for (std::size_t index = 0; index < depth; ++index) {
        std::size_t level = index + 1;
        std::size_t offset = index * 4;

        Price ask_price = parse_book_integer(fields[offset], level, "ask price");
        Quantity ask_size = parse_book_integer(fields[offset + 1], level, "ask size");
        Price bid_price = parse_book_integer(fields[offset + 2], level, "bid price");
        Quantity bid_size = parse_book_integer(fields[offset + 3], level, "bid size");

        if (ask_size < 0) {
            throw malformed_book_field(level, "ask size", "negative quantity");
        }
        if (bid_size < 0) {
            throw malformed_book_field(level, "bid size", "negative quantity");
        }

        if (ask_price == kEmptyAskPrice) {
            if (ask_size != 0) {
                throw malformed_book_field(
                    level, "ask price", "empty-level sentinel requires size 0"
                );
            }
            ask_sentinel_seen = true;
        } else {
            if (ask_price == kEmptyBidPrice) {
                throw malformed_book_field(
                    level, "ask price", "wrong-side empty-level sentinel"
                );
            }
            if (ask_price <= 0) {
                throw malformed_book_field(level, "ask price", "price must be positive");
            }
            if (ask_size == 0) {
                throw malformed_book_field(
                    level, "ask size", "occupied level requires positive quantity"
                );
            }
            if (ask_sentinel_seen) {
                throw malformed_book_field(
                    level, "ask price", "occupied level follows empty-level sentinel"
                );
            }
            if (have_previous_ask && ask_price <= previous_ask) {
                throw malformed_book_field(
                    level, "ask price", "ask prices must be strictly increasing"
                );
            }

            row.asks.push_back({ask_price, ask_size});
            previous_ask = ask_price;
            have_previous_ask = true;
        }

        if (bid_price == kEmptyBidPrice) {
            if (bid_size != 0) {
                throw malformed_book_field(
                    level, "bid price", "empty-level sentinel requires size 0"
                );
            }
            bid_sentinel_seen = true;
        } else {
            if (bid_price == kEmptyAskPrice) {
                throw malformed_book_field(
                    level, "bid price", "wrong-side empty-level sentinel"
                );
            }
            if (bid_price <= 0) {
                throw malformed_book_field(level, "bid price", "price must be positive");
            }
            if (bid_size == 0) {
                throw malformed_book_field(
                    level, "bid size", "occupied level requires positive quantity"
                );
            }
            if (bid_sentinel_seen) {
                throw malformed_book_field(
                    level, "bid price", "occupied level follows empty-level sentinel"
                );
            }
            if (have_previous_bid && bid_price >= previous_bid) {
                throw malformed_book_field(
                    level, "bid price", "bid prices must be strictly decreasing"
                );
            }

            row.bids.push_back({bid_price, bid_size});
            previous_bid = bid_price;
            have_previous_bid = true;
        }
    }

    return row;
}

}  // namespace lob
