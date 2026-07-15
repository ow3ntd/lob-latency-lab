# lob-latency-lab

[![CI](https://github.com/ow3ntd/lob-latency-lab/actions/workflows/ci.yml/badge.svg)](https://github.com/ow3ntd/lob-latency-lab/actions/workflows/ci.yml)

A C++20 limit order book and matching engine built for measurable low-latency performance: contiguous slot-pool storage, depth-independent cancellation, percentile latency benchmarks, and replay of real NASDAQ (LOBSTER) market data.

## Highlights

- **Depth-independent cancellation.** Replaced an O(n) vector-scan cancel with an intrusive doubly-linked list backed by a contiguous slot pool. Per-cancel cost dropped from ~46,700 ns to ~26 ns at 80,000 resting orders — flat across all book depths.
- **Percentile latency, not just averages.** Dedicated benchmarks report p50 / p95 / p99 per-event latency alongside aggregate throughput (~24.5M events/sec on the synthetic workload).
- **Validated on real market data.** Replays the full LOBSTER AAPL sample session (400,391 NASDAQ events) with consistent order-id bookkeeping and a coherent one-tick final spread.
- **Engineering process on record.** A [development log](docs/devlog.md) documents design decisions, including a cancellation optimization that was benchmarked, found slower, reverted, re-diagnosed, and then redesigned correctly.

## What This Is (and Isn't)

This project implements the systems layer of a simplified electronic exchange: order handling, price-time priority matching, trade generation, market data replay, testing, and latency measurement. It is not a trading strategy or price predictor — the focus is the infrastructure that strategies run on.

## Architecture

```
Market data                          Benchmarks
─────────────                        ──────────────────────────────
data/*.csv (synthetic)               bench_order_book        (throughput)
LOBSTER message file (real)          bench_order_book_latency (p50/p95/p99)
        │                            bench_deep_order_book   (deep book)
        ▼                            bench_cancel_stress     (cancel scaling)
MarketDataReplay.cpp                         │
  ├── replay_market_data()                   ▼
  └── replay_lobster_data()          Synthetic in-memory events
        │                                    │
        └──────────────┬─────────────────────┘
                       ▼
                 OrderBook.cpp
                   ├── add_order()      → price-time priority matching
                   ├── cancel_order()   → O(log P) lookup + O(1) unlink
                   ├── best_bid() / best_ask()
                   └── Trade generation
```

Input parsing lives entirely in `MarketDataReplay.cpp`, so the matching engine stays independent of data format and easy to test in isolation.

## Order Book Design

- **Price levels:** `std::map<Price, PriceLevel>` per side (bids descending, asks ascending), giving ordered best-price access.
- **Order storage:** all orders live in a single contiguous `std::vector<OrderNode>` slot pool. Each node carries `prev`/`next` indices into the same vector, forming an intrusive doubly-linked FIFO queue per price level — no per-node heap allocation, no pointer chasing across scattered allocations.
- **Cancellation:** an `order_id → slot index` hash map locates the node directly; the node is unlinked in place. Complexity is O(log P) for the price-level map lookup plus O(1) for the unlink, where P is the number of distinct price levels — not literally O(1) end to end, and the docs are careful about that distinction.
- **Slot reuse:** a free-list recycles cancelled slots, so heavy add/cancel churn allocates no new memory once the pool is warm.

The first attempt at direct-index cancellation used `std::list` and was *slower* than the O(n) scan it replaced (heap allocation per node destroyed cache locality). The slot-pool design keeps the O(1) unlink while preserving the contiguous layout that made the original vector fast. The full diagnosis is in the [devlog](docs/devlog.md).

## Performance

All results measured on a MacBook Pro, Apple clang++, `-O3 -DNDEBUG`. Full methodology and limitations for each benchmark are in [`results/`](results/), with baselines tracked over time in [`results/performance_history.md`](results/performance_history.md).

### Cancellation scaling (2026-07-07, slot-pool redesign)

`bench_cancel_stress` rests N orders at one price level and cancels all of them front-to-back — the worst case for a position-shifting cancel.

| Resting orders | Old vector cancel | New slot-pool cancel |
|---:|---:|---:|
| 5,000 | ~1,960 ns | ~29 ns |
| 10,000 | ~3,953 ns | ~24 ns |
| 20,000 | ~7,923 ns | ~25 ns |
| 40,000 | ~15,858 ns | ~27 ns |
| 80,000 | ~46,703 ns | ~26 ns |

The old cost doubles as the book doubles (the O(n) signature); the new cost is flat. The speedup grows with depth — ~68x at 5,000 orders to ~1,760x at 80,000 — which is what removing an algorithmic bottleneck looks like, rather than a constant-factor tweak.

### Throughput (1,000,000-event synthetic workload)

| Metric | Result (avg of 3 runs) |
|---|---:|
| Events per second | ~24.5M |
| Avg cost per event | ~40.8 ns |
| Trades generated | 250,000 |

### Per-event latency (100,000-event workload)

| Percentile | Latency |
|---|---:|
| p50 | 42 ns |
| p95 | 84 ns |
| p99 | 84 ns |

## Real Market Data: LOBSTER Replay

The engine replays [LOBSTER](https://lobsterdata.com) message files — reconstructed NASDAQ TotalView-ITCH order flow. The reader maps LOBSTER message types onto book operations (new orders, partial cancels, deletions), counts already-resolved executions rather than re-matching them, and parses fixed-point timestamps without floating point.

Replaying the full AAPL 2012-06-21 regular session:

| Metric | Value |
|---|---:|
| Events processed | 400,391 |
| Successful cancels | 152,223 / 174,386 cancel-type events |
| Final book | bid $577.56 / ask $577.57 (one-tick spread) |

LOBSTER's academic license does not permit redistribution, so the raw data file is excluded from version control. A small synthetic sample in the same format (`data/sample_lobster_message.csv`) is committed for CI, and [`results/lobster_replay_summary.md`](results/lobster_replay_summary.md) documents how to reproduce the full-session numbers.

## Build and Test

Requires CMake ≥ 3.20 and a C++20 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/test_order_book
./build/test_market_data_replay
```

Or run the full build + test + benchmark pipeline:

```bash
./scripts/run_all.sh
```

Every push builds all targets and runs both test suites plus smoke tests of all four benchmarks and the LOBSTER demo in [CI](.github/workflows/ci.yml).

## Benchmarks

```bash
./build/bench_order_book 1000000          # aggregate throughput
./build/bench_order_book_latency 100000   # p50 / p95 / p99 per-event latency
./build/bench_deep_order_book 1000000     # deep multi-level book workload
./build/bench_cancel_stress               # cancel cost vs. book depth
./build/lob_lobster_demo data/sample_lobster_message.csv
```

## Testing

Tests are dependency-free C++ `assert`-based suites covering:

- non-crossing adds, partial fills, full fills with remainder, both aggressor sides
- cancel of existing, missing, and already-filled orders
- volume conservation and non-negative resting quantities
- linked-list edge cases introduced by the slot-pool design: cancelling head, tail, middle (FIFO priority preserved), and sole order at a level
- slot reuse under heavy add/cancel churn
- CSV and LOBSTER replay parsing via in-memory streams

## Development Log

Design decisions, benchmark-driven experiments (including the reverted one), and honest limitations are documented chronologically in [`docs/devlog.md`](docs/devlog.md).

## Roadmap

- [x] Core order book: add, cancel, price-time priority matching, trade generation
- [x] Correctness test suites
- [x] CSV market data replay
- [x] Throughput and percentile latency benchmarks
- [x] O(log P) + O(1) cancellation via contiguous slot pool
- [x] Real-data replay (LOBSTER / NASDAQ)
- [x] CI: build, test, and benchmark smoke tests on every push
- [ ] Lock-free SPSC queue between data ingestion and matching thread
- [ ] Arena allocation for trade output on the hot path
- [ ] Re-baseline throughput/latency benchmarks post slot-pool redesign
