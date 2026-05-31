# lob-latency-lab

A C++ limit order book and matching engine focused on correctness, clean systems design, and measurable performance.

## Project Purpose

This project implements the core infrastructure behind a simplified electronic exchange. Rather than building a trading strategy or price predictor, it focuses on the systems layer: order handling, matching logic, trade generation, testing, and eventually latency benchmarking.

## Current Features

- Buy and sell limit order support
- Best bid and best ask tracking
- Order cancellation by order identifier
- Crossing-order matching
- Partial and full fill handling
- Trade event generation
- Correctness tests for core order book behavior

## Example Matching Scenario

```text
Resting order:   SELL 100 @ 10060
Incoming order: BUY   50 @ 10060

Result:
TRADE price=10060 quantity=50
Remaining ask: SELL 50 @ 10060
```

## Current Roadmap

- [x] Define order and trade types
- [x] Implement baseline order book operations
- [x] Implement basic matching logic
- [x] Add correctness tests
- [ ] Add comma-separated value market data replay
- [ ] Add latency benchmark runner
- [ ] Add benchmark report
- [ ] Add architecture diagram

## Running Tests

The project currently uses simple C++ `assert`-based correctness tests with no external testing framework.

```bash
mkdir -p build

clang++ -std=c++20 -Iinclude \
  tests/test_order_book.cpp src/OrderBook.cpp src/MatchingEngine.cpp \
  -o build/test_order_book

./build/test_order_book
```

Expected output:

```text
All order book tests passed.
```

## Current Test Coverage

The order book tests currently cover:

- adding non-crossing buy and sell orders
- canceling an existing order
- canceling a missing order
- crossing buy partial fill
- crossing buy full fill with remainder
- crossing sell partial fill

## Current Matching Behavior

The order book now generates trades when incoming orders cross resting orders:

- an incoming buy order matches when its price is greater than or equal to the best ask
- an incoming sell order matches when its price is less than or equal to the best bid
- partially filled resting orders remain in the book
- fully filled resting orders are removed from the book