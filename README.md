# lob-latency-lab

[![CI](https://github.com/ow3ntd/lob-latency-lab/actions/workflows/ci.yml/badge.svg)](https://github.com/ow3ntd/lob-latency-lab/actions/workflows/ci.yml)

A C++20 limit order book and matching engine built for measurable low-latency performance: contiguous slot-pool storage, depth-independent cancellation, percentile latency benchmarks, and replay of real NASDAQ (LOBSTER) market data.

## Highlights

- **Flat cancellation scaling over tested depths.** Replaced O(n) vector shifting within a price level with an intrusive doubly-linked list backed by a contiguous slot pool. Current local synthetic measurements show approximately 14–18 ns median cancellation cost across 5,000 to 80,000 resting orders.
- **Percentile latency, not just averages.** Dedicated benchmarks report p50 / p95 / p99 per-event latency alongside aggregate throughput. Historical baselines are retained with their implementation context in [`results/`](results/).
- **Real-data validation path.** Parses LOBSTER message files and validates each local transition against the paired order-book snapshots. On the official AAPL Level-10 sample, 295,828 transitions matched exactly, 104,562 were conservatively classified as unverifiable tail refills, and zero observable mismatches were found.
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

The current measurements are local synthetic microbenchmarks from a CMake
Release build on an Apple M4 MacBook Pro. Each executable had one discarded
warm-up followed by five measured trials. The machine was otherwise
uncontrolled.

| Metric | Current five-run median |
|---|---:|
| Aggregate throughput | ~33.9M events/s |
| Aggregate average cost | ~29.5 ns/event |
| Per-event p50 latency | 42 ns |
| Per-event p95 latency | 83 ns |
| Per-event p99 latency | 84 ns |
| Cancellation cost, 5,000–80,000 resting orders | ~14–18 ns median |

These results characterize deterministic workloads on one local machine. They
are not production exchange measurements and are not intended for
cross-machine comparison. See
[`results/current_performance.md`](results/current_performance.md) for the
environment, commands, raw trials, ranges, methodology, and workload details.
Historical baselines remain in
[`results/performance_history.md`](results/performance_history.md).

The approximately flat cancellation medians are consistent with the slot
pool's O(1) unlink operation. The complete cancellation API also performs
identifier and price-map lookups, so it is not claimed to be mathematically
constant-time in every component.

### Historical cancellation scaling (2026-07-07)

`bench_cancel_stress` rests N orders at one price level and cancels all of them front-to-back — the worst case for a position-shifting cancel.

| Resting orders | Old vector cancel | New slot-pool cancel |
|---:|---:|---:|
| 5,000 | ~1,960 ns | ~29 ns |
| 10,000 | ~3,953 ns | ~24 ns |
| 20,000 | ~7,923 ns | ~25 ns |
| 40,000 | ~15,858 ns | ~27 ns |
| 80,000 | ~46,703 ns | ~26 ns |

The old cost doubles as the book doubles (the O(n) signature); the new cost is flat. The speedup grows with depth — ~68x at 5,000 orders to ~1,760x at 80,000 — which is what removing an algorithmic bottleneck looks like, rather than a constant-factor tweak.

### Historical aggregate baseline

The following aggregate and percentile figures predate the slot-pool redesign.

| Metric | Historical result (avg of 3 runs) |
|---|---:|
| Events per second | ~24.5M |
| Avg cost per event | ~40.8 ns |
| Trades generated | 250,000 |

### Per-event latency (100,000-event workload)

| Percentile | Historical latency |
|---|---:|
| p50 | 42 ns |
| p95 | 84 ns |
| p99 | 84 ns |

## Real Market Data: LOBSTER Replay

The engine replays [LOBSTER](https://lobsterdata.com) message files—reconstructed NASDAQ TotalView-ITCH order flow. The reader handles message types as follows:

- type 1 adds a visible resting order
- type 2 reduces the referenced order in place
- type 3 deletes the referenced order
- type 4 reduces the referenced resting quantity without generating a new match
- type 5 counts a hidden execution without mutating the visible book
- type 6 counts a cross trade without mutating the visible book
- type 7 counts an auxiliary or halt event without mutating the visible book

LOBSTER timestamps are parsed with fixed-point arithmetic rather than floating point.

The project provides two paired-file validators with different starting
assumptions:

- `lob_lobster_validate` attempts full reconstruction from an empty
  `OrderBook`, applying every message before comparing the resulting snapshot.
- `lob_lobster_transition_validate` treats order-book row 1 as an authoritative
  baseline and checks whether message row k explains the transition from
  order-book row k-1 to row k.

The local transition model is necessary because the regular-session LOBSTER
message file does not contain all resting orders established before row 1.
Consequently, it cannot reconstruct the first regular-session snapshot from an
empty book.

Run the empty-start validator against paired files with:

```bash
./build/lob_lobster_validate <message.csv> <orderbook.csv> <depth>
```

Run the local transition validator with:

```bash
./build/lob_lobster_transition_validate <message.csv> <orderbook.csv> <depth>
```

### AAPL Level-10 transition-validation result

| Classification | Count | Percentage |
|---|---:|---:|
| Exact matching transitions | 295,828 | 73.88% |
| Unverifiable tail refills | 104,562 | 26.12% |
| Observable mismatches | 0 | 0.00% |
| Total transitions checked | 400,390 | 100.00% |

The run read 400,391 paired rows: one authoritative baseline and 400,390
checked transitions. Tail refills are conservative classifications, not
mismatches: when a visible top-10 level disappears, a previously hidden level
may enter position 10 even though its prior price and quantity were unavailable
to the validator. The validator checks the known prefix but does not claim to
validate that unknown depth.

See
[`results/lobster_transition_validation.md`](results/lobster_transition_validation.md)
for the dataset identity, environment, exact command, complete counters, and
limitations.

LOBSTER's academic license does not permit redistribution, so the raw data
files are excluded from version control. Small synthetic files in the same
formats (`data/sample_lobster_message.csv` and
`data/sample_lobster_orderbook_2.csv`) are committed as reproducible examples.
[`results/lobster_replay_summary.md`](results/lobster_replay_summary.md)
documents the historical message-only replay, its limitations, and the command
used to run it.

The full-session numbers in the historical replay report predate the corrected
type-2 and type-4 quantity semantics. They are retained only as a historical
parser-coverage run and must not be treated as current reconstruction-validation
results.

## Build and Test

Requires CMake ≥ 3.20 and a C++20 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure --no-tests=error
```

The convenience script performs a Release configure and build, runs the two test executables, and runs the aggregate-throughput benchmark:

```bash
./scripts/run_all.sh
```

On pushes and pull requests to `main`, [CI](.github/workflows/ci.yml) builds all CMake targets, runs registered tests through CTest, fails if no tests are registered, and runs one aggregate benchmark smoke test. CI does not enforce performance thresholds.

## Benchmarks

```bash
./build/bench_order_book 1000000          # aggregate throughput
./build/bench_order_book_latency 100000   # p50 / p95 / p99 per-event latency
./build/bench_deep_order_book 1000000     # deep multi-level book workload
./build/bench_cancel_stress               # cancel cost vs. book depth
./build/lob_lobster_demo data/sample_lobster_message.csv
```

## Testing

Tests are dependency-free suites using an always-active `CHECK` mechanism that remains enabled in Debug and Release builds. Coverage includes:

- non-crossing adds, partial fills, full fills with remainder, both aggressor sides
- cancel of existing, missing, and already-filled orders
- volume conservation and non-negative resting quantities
- linked-list edge cases introduced by the slot-pool design: cancelling head, tail, middle (FIFO priority preserved), and sole order at a level
- slot reuse under heavy add/cancel churn
- CSV and LOBSTER replay parsing via in-memory streams
- paired LOBSTER full-replay and local transition-validation behavior

## Development Log

Design decisions, benchmark-driven experiments (including the reverted one), and honest limitations are documented chronologically in [`docs/devlog.md`](docs/devlog.md).

## Roadmap

- [x] Core order book: add, cancel, price-time priority matching, trade generation
- [x] Correctness test suites
- [x] CSV market data replay
- [x] Throughput and percentile latency benchmarks
- [x] O(log P) + O(1) cancellation via contiguous slot pool
- [x] LOBSTER message-file replay
- [x] Paired LOBSTER order-book snapshot and local transition validation
- [x] CMake/CTest CI: build all targets, require registered tests, and run one aggregate benchmark smoke test
- [x] Re-baseline throughput/latency benchmarks post slot-pool redesign

### Optional future experiments

These are outside the completed v1 scope:

- Lock-free SPSC ingestion between dedicated producer and matching threads
- Arena-backed trade-result allocation
