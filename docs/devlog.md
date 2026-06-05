# Development Log

## 2026-05-24

Created the initial repository structure for `lob-latency-lab`.

I decided to start with a design document before writing implementation code so that the project shows engineering planning instead of looking like a one-shot generated project.

Initial scope:
- define basic order and trade types
- implement order book add/cancel/match logic
- add tests before optimization
- add benchmarks after correctness is working

Next step: define the core C++ domain types for orders, trades, prices, quantities, and sides.

## 2026-05-29 — Basic Matching Logic Added

Implemented basic crossing-order matching.

Current behavior:
- incoming buy orders match resting sell orders when buy price is greater than or equal to the best ask
- incoming sell orders match resting buy orders when sell price is less than or equal to the best bid
- trade events are generated with resting order ID, aggressive order ID, price, quantity, and timestamp
- partial fills are supported
- fully filled resting orders are removed from the book

I verified the demo with a buy order crossing a resting sell order. The program generated one trade at price 10060 with quantity 50.

Next step: add unit tests for add, cancel, partial fill, full fill, and non-crossing orders.

## 2026-05-29 — Correctness Tests Added

Added unit-style correctness tests for the order book.

Test coverage now includes:
- adding non-crossing buy and sell orders
- canceling an existing order
- canceling a missing order
- crossing buy partial fill
- crossing buy full fill with remainder
- crossing sell partial fill

The tests are currently written with standard C++ `assert` statements so the project stays simple and easy to compile without external dependencies.

Next step: add more edge-case tests, then add market data replay from CSV.

## 2026-06-03 — CSV Market Data Replay Added

Implemented CSV-based market data replay as a separate layer from the matching engine.

New behavior:
- reads ADD and CANCEL events from CSV input
- converts BUY and SELL text values into order book events
- replays events through the existing matching engine
- reports processed event counts, successful cancellations, generated trades, and total traded quantity
- supports in-memory replay tests using `std::istringstream`

I kept replay parsing separate from `OrderBook.cpp` so the matching engine remains independent of the input format and easier to test.

Verified sample replay:
- 4 events processed
- 3 add events
- 1 cancel event
- 1 generated trade
- total traded quantity of 50
- final remaining ask at price 10060

Next step: add latency and throughput benchmarking for replayed order events.
## 2026-06-04 — Benchmark Summary Added

Added a benchmark report for the deterministic order book benchmark.

The benchmark processes 1,000,000 synthetic events using a repeated add, match, and cancel pattern. The workload is generated in memory before timing starts so the benchmark measures order book processing rather than CSV parsing or file input.

Across three runs, the benchmark processed approximately 24.5 million events per second, with an average processing cost of about 40.8 nanoseconds per event.

I documented the benchmark design, build command, run command, results table, and measurement limitations in `results/benchmark_summary.md`.

Next step: add an architecture diagram and improve the README so the project is easier for recruiters to understand quickly.