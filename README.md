# lob-latency-lab

A C++20 limit order book and matching engine focused on correctness, clean systems design, and measured latency/throughput tradeoffs.

## Why This Exists

This project is a small systems lab for the core mechanics behind an electronic exchange: order storage, cancellation, matching, trade generation, replay, testing, and benchmarking. It is intentionally scoped to market-structure infrastructure rather than trading strategy or price prediction.

## Current Features

- Buy and sell limit order support
- Integer price ticks for deterministic price comparisons
- Best bid and best ask tracking
- Order cancellation by order identifier
- Crossing-order matching
- Partial and full fill handling
- Trade event generation
- CSV market data replay separated from matching logic
- Assert-based correctness tests
- Throughput, latency, and deeper-book benchmark scenarios

## Architecture

```text
data/sample_orders.csv
        |
        v
MarketDataReplay
        |
        v
OrderBook
        |-- add_order(...)
        |-- cancel_order(...)
        |-- best_bid()
        |-- best_ask()
        v
Trades + ReplaySummary
```

```text
Synthetic benchmark events
        |
        v
OrderBook
        |
        v
Throughput / latency metrics
```

Key implementation boundaries:

- `OrderBook` owns order storage, cancellation, matching, and best-price queries.
- `MarketDataReplay` owns CSV parsing and replay so input handling stays independent from matching logic.
- Benchmark programs generate in-memory events to avoid measuring CSV parsing or console output in the timed path.

## Benchmark Snapshot

Local benchmark snapshot:

| Scenario | Throughput | Average cost | Latency |
| --- | ---: | ---: | ---: |
| Small book throughput | ~24.5 million events/sec | ~40.8 ns/event | Not measured in throughput loop |
| Small book latency | Separate per-event timing run | Not primary metric | p50 42 ns, p99 84 ns |
| Deep book throughput | ~178K events/sec | ~5,624.72 ns/event | Not measured in throughput loop |

Detailed methodology and historical notes live in:

- `results/benchmark_summary.md`
- `results/latency_benchmark_summary.md`
- `results/deep_book_benchmark_summary.md`
- `results/performance_history.md`

## Build And Run Tests

The project uses CMake and simple `assert`-based tests.

```bash
cmake -S . -B build
cmake --build build

./build/test_order_book
./build/test_market_data_replay
```

Expected test output includes:

```text
All order book tests passed.
All market data replay tests passed.
```

## Run Benchmarks

```bash
cmake --build build --target bench_order_book
cmake --build build --target bench_order_book_latency
cmake --build build --target bench_deep_order_book

./build/bench_order_book
./build/bench_order_book_latency
./build/bench_deep_order_book
```

## Key Engineering Lessons

- Integer price ticks avoid floating-point price comparison issues.
- CSV replay is separated from matching logic so input parsing is independent from the order book.
- Throughput and per-event latency are benchmarked separately because timing every event adds overhead.
- A list-based direct-cancel experiment was reverted after benchmarks showed worse performance due to cache locality and allocation/pointer-chasing costs.
- A deeper-book benchmark was added to stress larger book states beyond the small deterministic benchmark.

## Limitations

- This is a simplified educational order book, not a production trading system.
- Matching is single-threaded.
- Order types are limited to basic limit orders and cancellations.
- Persistence, networking, risk checks, and exchange connectivity are out of scope.
- Benchmarks are local measurements and should be interpreted with the documented methodology.

## Roadmap

- Expand correctness and invariant tests around edge cases.
- Add broader replay scenarios and malformed-input coverage.
- Compare alternative data structures under both small-book and deep-book workloads.
- Track benchmark results over time as implementation details change.
- Add clearer architecture notes for design tradeoffs and benchmark interpretation.
