#include "MarketDataReplay.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace lob {
namespace {

struct ParsedLobsterMessage {
    Timestamp timestamp;
    long type;
    OrderId order_id;
    Quantity size;
    Price price;
    long direction;
};

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

ParsedLobsterMessage parse_lobster_message(
    const std::string& line,
    std::size_t row
) {
    auto fields = split_csv_line(line);

    if (fields.size() < 6) {
        throw malformed_row(row, "expected at least 6 fields");
    }

    return {
        parse_lobster_timestamp(fields[0], row),
        parse_lobster_int(fields[1], row, "type"),
        parse_order_id(fields[2], row),
        parse_quantity(fields[3], row),
        parse_price(fields[4], row),
        parse_lobster_int(fields[5], row, "direction")
    };
}

void apply_lobster_message(const ParsedLobsterMessage& message,
                           std::size_t row, OrderBook& book,
                           LobsterSummary& summary) {

    ++summary.events_processed;

    switch (message.type) {
        case 1: {
            Order order{
                message.order_id,
                lobster_direction_to_side(message.direction, row),
                message.price,
                message.size,
                message.timestamp
            };
            book.add_order(order);
            ++summary.new_orders;
            break;
        }
        case 2:
            ++summary.partial_cancels;
            switch (book.reduce_order(message.order_id, message.size)) {
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
        case 3:
            ++summary.deletions;
            if (book.cancel_order(message.order_id)) {
                ++summary.successful_cancels;
            }
            break;
        case 4:
            ++summary.executions;
            summary.executed_quantity += message.size;
            switch (book.reduce_order(message.order_id, message.size)) {
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
        case 5:
            ++summary.hidden_executions;
            break;
        case 6:
            ++summary.cross_trades;
            break;
        case 7:
            ++summary.auxiliary;
            break;
        default:
            throw malformed_row(row, "unknown LOBSTER event type");
    }
}

void apply_lobster_message_line(const std::string& line, std::size_t row,
                                OrderBook& book, LobsterSummary& summary) {
    apply_lobster_message(parse_lobster_message(line, row), row, book, summary);
}

void validate_transition_message_semantics(const ParsedLobsterMessage& message,
                                           std::size_t row) {
    switch (message.type) {
        case 1: case 2: case 3: case 4:
            if (message.direction != 1 && message.direction != -1) {
                throw malformed_row(row, "invalid direction");
            }
            if (message.size <= 0) {
                throw malformed_row(row, "quantity must be positive");
            }
            break;
        case 5: case 6: case 7:
            break;
        default:
            throw malformed_row(row, "unknown LOBSTER event type");
    }
}

void compare_lobster_side(
    const std::vector<LevelSnapshot>& expected,
    const std::vector<LevelSnapshot>& actual,
    LobsterBookSide side,
    std::size_t row,
    LobsterValidationSummary& summary,
    bool& row_matches
) {
    std::size_t level_count = std::max(expected.size(), actual.size());

    for (std::size_t index = 0; index < level_count; ++index) {
        std::optional<LevelSnapshot> expected_level;
        std::optional<LevelSnapshot> actual_level;

        if (index < expected.size()) {
            expected_level = expected[index];
        }
        if (index < actual.size()) {
            actual_level = actual[index];
        }

        bool level_mismatch = false;
        if (!expected_level || !actual_level) {
            ++summary.missing_level_mismatches;
            level_mismatch = true;
        } else {
            if (expected_level->price != actual_level->price) {
                ++summary.price_mismatches;
                level_mismatch = true;
            }
            if (expected_level->quantity != actual_level->quantity) {
                ++summary.quantity_mismatches;
                level_mismatch = true;
            }
        }

        if (level_mismatch) {
            row_matches = false;
            if (!summary.first_mismatch) {
                summary.first_mismatch = LobsterValidationMismatch{
                    row, side, index + 1, expected_level, actual_level
                };
            }
        }
    }
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

        apply_lobster_message_line(line, line_number, book, summary);
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

LobsterValidationSummary validate_lobster_replay(
    std::istream& message_input,
    std::istream& orderbook_input,
    OrderBook& book,
    std::size_t depth
) {
    if (depth == 0) {
        throw std::runtime_error("LOBSTER validation depth must be greater than zero");
    }
    if (book.order_count() != 0) {
        throw std::runtime_error(
            "LOBSTER validation requires an empty initial order book"
        );
    }

    LobsterValidationSummary summary;
    std::string message_line;
    std::string orderbook_line;
    std::size_t row = 0;

    while (true) {
        bool has_message = static_cast<bool>(
            std::getline(message_input, message_line)
        );
        bool has_orderbook = static_cast<bool>(
            std::getline(orderbook_input, orderbook_line)
        );

        if (!has_message && !has_orderbook) {
            break;
        }

        ++row;
        if (!has_message) {
            throw std::runtime_error(
                "LOBSTER stream length mismatch at row " + std::to_string(row) +
                ": message stream ended before order-book stream"
            );
        }
        if (!has_orderbook) {
            throw std::runtime_error(
                "LOBSTER stream length mismatch at row " + std::to_string(row) +
                ": order-book stream ended before message stream"
            );
        }

        try {
            apply_lobster_message_line(message_line, row, book, summary.replay);
        } catch (const std::runtime_error& error) {
            throw std::runtime_error(
                "Malformed LOBSTER message stream at row " +
                std::to_string(row) + ": " + error.what()
            );
        }

        LobsterBookRow expected;
        try {
            expected = parse_lobster_book_row(orderbook_line, depth);
        } catch (const std::runtime_error& error) {
            throw std::runtime_error(
                "Malformed LOBSTER order-book stream at row " +
                std::to_string(row) + ": " + error.what()
            );
        }

        BookSnapshot actual = book.snapshot(depth);
        bool row_matches = true;
        compare_lobster_side(
            expected.bids, actual.bids, LobsterBookSide::Bid,
            row, summary, row_matches
        );
        compare_lobster_side(
            expected.asks, actual.asks, LobsterBookSide::Ask,
            row, summary, row_matches
        );

        ++summary.rows_compared;
        if (row_matches) {
            ++summary.matching_rows;
        } else {
            ++summary.mismatching_rows;
        }
    }

    return summary;
}

LobsterTransitionSummary validate_lobster_transitions(
    std::istream& message_input, std::istream& orderbook_input,
    std::size_t depth) {
    if (depth == 0) throw std::runtime_error("LOBSTER transition validation depth must be greater than zero");
    LobsterTransitionSummary s;
    std::optional<LobsterBookRow> previous;
    std::string ml, bl;
    std::size_t row = 0;
    while (true) {
        bool hm = static_cast<bool>(std::getline(message_input, ml));
        bool hb = static_cast<bool>(std::getline(orderbook_input, bl));
        if (!hm && !hb) break;
        ++row;
        if (!hm) throw std::runtime_error("LOBSTER stream length mismatch at row " + std::to_string(row) + ": message stream ended before order-book stream");
        if (!hb) throw std::runtime_error("LOBSTER stream length mismatch at row " + std::to_string(row) + ": order-book stream ended before message stream");
        ParsedLobsterMessage m;
        try { m = parse_lobster_message(ml, row); validate_transition_message_semantics(m, row); }
        catch (const std::runtime_error& e) { throw std::runtime_error("Malformed LOBSTER message stream at row " + std::to_string(row) + ": " + e.what()); }
        LobsterBookRow current;
        try { current = parse_lobster_book_row(bl, depth); }
        catch (const std::runtime_error& e) { throw std::runtime_error("Malformed LOBSTER order-book stream at row " + std::to_string(row) + ": " + e.what()); }
        ++s.rows_read;
        if (!previous) { previous = std::move(current); ++s.baseline_rows; continue; }
        ++s.transitions_checked;
        enum class C { Match, Mismatch, Unsupported, Unverifiable } cls = C::Match;
        auto side = m.direction == 1 ? LobsterBookSide::Bid : LobsterBookSide::Ask;
        auto levels = [](const LobsterBookRow& r, LobsterBookSide x) -> const std::vector<LevelSnapshot>& { return x == LobsterBookSide::Bid ? r.bids : r.asks; };
        auto other = side == LobsterBookSide::Bid ? LobsterBookSide::Ask : LobsterBookSide::Bid;
        auto capture = [&](LobsterBookSide x, std::size_t level, const char* reason, std::optional<LevelSnapshot> e, std::optional<LevelSnapshot> a) {
            if (!s.first_mismatch) s.first_mismatch = LobsterTransitionMismatch{row,m.type,x,level,m.price,m.size,reason,e,a};
        };
        auto compare = [&](const std::vector<LevelSnapshot>& e, const std::vector<LevelSnapshot>& a, LobsterBookSide x, const char* override_reason = nullptr) {
            bool ok = true; std::size_t n = std::max(e.size(), a.size());
            for (std::size_t i=0;i<n;++i) {
                std::optional<LevelSnapshot> ev = i<e.size()?std::optional<LevelSnapshot>(e[i]):std::nullopt;
                std::optional<LevelSnapshot> av = i<a.size()?std::optional<LevelSnapshot>(a[i]):std::nullopt;
                if (!ev || !av) { ++s.missing_level_mismatches; ok=false; capture(x,i+1,override_reason?override_reason:(ev?"missing actual level":"unexpected actual level"),ev,av); continue; }
                if (ev->price != av->price) { ++s.price_mismatches; ok=false; capture(x,i+1,override_reason?override_reason:"price mismatch",ev,av); }
                if (ev->quantity != av->quantity) { ++s.quantity_mismatches; ok=false; capture(x,i+1,override_reason?override_reason:"quantity mismatch",ev,av); }
            } return ok;
        };
        if (m.type == 6 || m.type == 7) cls = C::Unsupported;
        else if (m.type == 5) {
            cls = compare(previous->bids,current.bids,LobsterBookSide::Bid) && compare(previous->asks,current.asks,LobsterBookSide::Ask) ? C::Match : C::Mismatch;
        } else {
            bool mismatch = false;
            if (!compare(levels(*previous,other),levels(current,other),other,"opposite side changed")) { ++s.opposite_side_changes; mismatch=true; }
            const auto& prev = levels(*previous,side); const auto& cur = levels(current,side);
            if (m.type == 1) {
                std::vector<LevelSnapshot> predicted=prev; auto it=std::find_if(predicted.begin(),predicted.end(),[&](auto& l){return l.price==m.price;});
                if(it!=predicted.end()) it->quantity+=m.size; else { auto pos=std::find_if(predicted.begin(),predicted.end(),[&](auto& l){return side==LobsterBookSide::Bid?m.price>l.price:m.price<l.price;}); predicted.insert(pos,{m.price,m.size}); }
                if(predicted.size()>depth) predicted.resize(depth); if(!compare(predicted,cur,side)) mismatch=true; cls=mismatch?C::Mismatch:C::Match;
            } else {
                auto it=std::find_if(prev.begin(),prev.end(),[&](auto& l){return l.price==m.price;});
                if(it==prev.end()) {
                    ++s.missing_event_price_levels;
                    if(prev.size()<depth) { capture(side,0,"event price absent from fully visible side",std::nullopt,std::nullopt); compare(prev,cur,side); cls=C::Mismatch; }
                    else { bool event_same=compare(prev,cur,side,"off-depth reduction cannot account for visible-book change"); cls=(!mismatch&&event_same)?C::Unverifiable:C::Mismatch; }
                } else if(m.size>it->quantity) { ++s.quantity_underflows; capture(side,static_cast<std::size_t>(it-prev.begin())+1,"event quantity exceeds previous visible level quantity",*it,std::nullopt); cls=C::Mismatch; }
                else { std::vector<LevelSnapshot> predicted=prev; auto index=static_cast<std::size_t>(it-prev.begin()); predicted[index].quantity-=m.size;
                    if(predicted[index].quantity>0) { if(!compare(predicted,cur,side)) mismatch=true; cls=mismatch?C::Mismatch:C::Match; }
                    else { predicted.erase(predicted.begin()+index);
                        if(prev.size()<depth) { if(!compare(predicted,cur,side)) mismatch=true; cls=mismatch?C::Mismatch:C::Match; }
                        else { bool count_ok=cur.size()==predicted.size()||cur.size()==predicted.size()+1; std::vector<LevelSnapshot> prefix(cur.begin(),cur.begin()+std::min(cur.size(),predicted.size())); bool prefix_ok=compare(predicted,prefix,side,"invalid known prefix after visible-level removal");
                            if(!count_ok||!prefix_ok||mismatch) cls=C::Mismatch; else if(cur.size()==predicted.size()+1) { ++s.tail_refill_transitions; cls=C::Unverifiable; } else cls=C::Match;
                        }
                    }
                }
            }
        }
        if(cls==C::Match)++s.matching_transitions; else if(cls==C::Mismatch)++s.mismatching_transitions; else if(cls==C::Unsupported)++s.unsupported_transitions; else ++s.unverifiable_transitions;
        previous=std::move(current);
    }
    return s;
}

}  // namespace lob
