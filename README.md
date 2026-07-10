# lob-latency-lab

![C++ CI](https://github.com/ow3ntd/lob-latency-lab/actions/workflows/ci.yml/badge.svg)

A C++20 limit order book and matching engine focused on correctness, clean systems design, and measured latency/throughput tradeoffs.

## Project Purpose

This project implements the core infrastructure behind a simplified electronic exchange. Rather than building a trading strategy or price predictor, it focuses on the systems layer: order handling, price-time priority matching, cancellation, trade generation, deterministic replay, correctness testing, and benchmarking.

The goal is to study how data structures, matching logic, replay design, and benchmark methodology affect the behavior of a low-latency C++ system.

## What This Project Is

* A simplified C++ limit order book and matching engine
* A systems project focused on correctness, determinism, and performance measurement
* A testbed for studying order matching, market data replay, and benchmark methodology
* A project intended to build toward Quant Dev / trading systems / performance engineering skills

## What This Project Is Not

This project is not a production-grade exchange matching engine, trading platform, or HFT system.

Current benchmarks are synthetic, in-memory microbenchmarks. They are designed to measure this project’s matching and replay behavior under controlled workloads. They do not include network I/O, disk I/O, exchange connectivity, kernel bypass, persistence, risk checks, fault tolerance, concurrency, or production deployment constraints.

Benchmark results should be interpreted as measurements of a controlled student systems project, not as claims about production trading-system latency.

## Architecture

```text
CSV order events / synthetic order events
        │
        ▼
MarketDataReplay
        │
        ▼
OrderBook
        │
        ├── add_order(...)
        ├── cancel_order(...)
        ├── best_bid()
        └── best_ask()
        │
        ▼
Matching logic
        │
        ▼
Trades + final book state + replay/benchmark summary
```

The project separates input handling, matching logic, and measurement:

* `MarketDataReplay.cpp` handles CSV parsing and deterministic replay.
* `OrderBook.cpp` handles order storage, cancellation, best bid/ask tracking, matching, and trade generation.
* `bench_order_book.cpp` measures synthetic in-memory throughput without CSV parsing or console printing inside the timed loop.
* `tests/test_order_book.cpp` verifies core matching and cancellation behavior.

## Market Data Replay

The engine replays two input formats through the same order book:

* A simple internal CSV format (`ADD`/`CANCEL`, `BUY`/`SELL`) used for tests and the synthetic benchmark.
* The real **LOBSTER** message format (6-column NASDAQ TotalView-ITCH events: time, type, order id, size, price, direction).

The LOBSTER reader (`replay_lobster_data`) parses real message files, mapping submissions, cancellations, and deletions onto the book. Because LOBSTER data is already reconstructed, executions and hidden liquidity are counted rather than re-matched — the goal is to exercise the engine on real NASDAQ event sequences and data shapes, not to reproduce NASDAQ's exact trades.

Replaying the AAPL 2012-06-21 sample processes 400,391 real events and ends with a coherent one-tick book (best bid/ask 5775600/5775700). Full results and reproduction steps are in `results/lobster_replay_summary.md`.

The LOBSTER sample data itself is **not committed** to this repository: it derives from NASDAQ TotalView-ITCH under an academic waiver that does not permit redistribution. Download the free sample from lobsterdata.com and run:

```bash
./build/lob_lobster_demo data/AAPL_2012-06-21_34200000_57600000_message_10.csv
```

## Current Features

* Buy and sell limit order support
* Best bid and best ask tracking
* Order cancellation by order identifier
* Crossing-order matching
* Partial and full fill handling
* Trade event generation
* CSV-based market data replay
* Real LOBSTER message-format replay (real NASDAQ event data)
* Synthetic in-memory benchmark runner
* Correctness tests for core order book behavior

## Benchmark Snapshot

Local benchmark snapshot:

| Scenario | Throughput | Average Cost | Latency |
|---|---:|---:|---:|
| Small-book throughput | ~24.5 million events/sec | ~40.8 ns/event | Not measured in throughput loop |
| Small-book latency | Separate per-event timing run | Not primary metric | p50 42 ns, p99 84 ns |
| Deep-book throughput | ~178K events/sec | ~5,624.72 ns/event | Not measured in throughput loop |

Detailed methodology and historical notes live in:

- `results/benchmark_summary.md`
- `results/latency_benchmark_summary.md`
- `results/deep_book_benchmark_summary.md`
- `results/performance_history.md`


## Example Matching Scenario

```text
Resting order:   SELL 100 @ 10060
Incoming order: BUY   50 @ 10060

Result:
TRADE price=10060 quantity=50
Remaining ask: SELL 50 @ 10060
```

## Current Roadmap

Completed:

* [x] Define order and trade types
* [x] Implement baseline order book operations
* [x] Implement best bid / best ask tracking
* [x] Implement cancellation by order identifier
* [x] Implement crossing-order matching
* [x] Handle partial and full fills
* [x] Generate trade events
* [x] Add CSV market data replay
* [x] Add synthetic benchmark runner
* [x] Add benchmark summary
* [x] Add correctness tests for core behavior

Near-term improvements:

* [ ] Add clearer benchmark methodology documentation
* [ ] Add baseline comparison against a naive `std::map` order book
* [ ] Add deterministic replay verification
* [ ] Expand correctness tests for price-time priority and edge cases
* [ ] Add p50/p95/p99 latency benchmark reporting
* [ ] Add design notes explaining data structure tradeoffs
* [ ] Add a cleaner architecture diagram
* [ ] Add one-command build/test/benchmark script


## Reproducing Locally

From the repository root:

```bash
./scripts/run_all.sh
```

This configures a Release CMake build, builds the project, runs correctness tests, and runs the current benchmark.

## Running Tests

The project currently uses simple C++ `assert`-based correctness tests with no external testing framework.

```bash
mkdir -p build

clang++ -std=c++20 -Iinclude \
  src/OrderBook.cpp \
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

