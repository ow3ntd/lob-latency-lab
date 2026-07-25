# Current Performance Baseline

## Scope

This report records the post-slot-pool performance baseline for the current
order-book implementation. The measurements are synthetic microbenchmarks
intended to characterize these specific deterministic workloads on one local
machine.

They are not measurements of production exchange performance and should not be
used for cross-machine comparisons.

## Benchmark Commit and Environment

Benchmark code commit:

```text
f9d629936d8cc04278434ac77f8b4baf5aed13bd
```

| Field | Value |
|---|---|
| Compiler | Apple clang 21.0.0 (`clang-2100.0.123.102`) |
| Operating system | macOS 26.2, build 25C56 |
| Architecture | arm64 |
| Hardware | MacBook Pro (Mac16,1) |
| Processor | Apple M4, 10 cores |
| Memory | 24 GB |
| Build type | CMake Release |

## Build and Test

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure --no-tests=error
```

All four registered CTest tests passed before the benchmark runs.

The measured benchmark commands were:

```bash
./build-release/bench_order_book 1000000
./build-release/bench_deep_order_book 1000000
./build-release/bench_order_book_latency 100000
./build-release/bench_cancel_stress
```

## Methodology

Each benchmark executable was run once as a discarded warm-up, then five more
times for measurement. The tables below report those five measured trials.
Summary values use the median and the full observed minimum-to-maximum range.

The machine was otherwise uncontrolled. Normal operating-system activity,
scheduling, interrupts, power management, and other local processes could
affect the results.

These benchmarks use synthetic, deterministic event streams:

- The aggregate benchmark repeatedly adds a resting order, adds a non-crossing
  order that will later be cancelled, adds a crossing order, and cancels the
  designated resting order. Buy- and sell-aggressor cycles alternate.
- The deep-book benchmark first seeds 100 price levels on each side with 20
  orders per level. Its measured eight-event cycle combines deep-level adds and
  cancellations with best-price crossings and replenishments.
- The percentile-latency benchmark uses the aggregate benchmark's four-event
  cycle but times each event individually.
- The cancellation-stress benchmark rests every order at one price and
  cancels the orders front-to-back. This isolates the scaling behavior that
  previously caused vector element shifts.

The aggregate and deep-book results should not be interpreted as a universal
head-to-head speed comparison. They exercise different deterministic
workloads, initial book states, and event mixes.

## Aggregate Throughput

Each trial processed 1,000,000 events.

### Raw trials

| Trial | Throughput (events/s) | Average cost (ns/event) |
|---:|---:|---:|
| 1 | 34,330,700 | 29.1284 |
| 2 | 32,501,100 | 30.7682 |
| 3 | 35,025,900 | 28.5503 |
| 4 | 33,809,100 | 29.5778 |
| 5 | 33,870,600 | 29.5241 |

### Summary

| Metric | Median | Observed range |
|---|---:|---:|
| Throughput | 33,870,600 events/s | 32,501,100–35,025,900 events/s |
| Average cost | 29.5241 ns/event | 28.5503–30.7682 ns/event |

## Deep-Book Throughput

Each trial processed 1,000,000 measured events after constructing the seeded
book.

### Raw trials

| Trial | Throughput (events/s) | Average cost (ns/event) |
|---:|---:|---:|
| 1 | 34,464,600 | 29.0153 |
| 2 | 33,343,200 | 29.9911 |
| 3 | 35,479,000 | 28.1857 |
| 4 | 34,763,000 | 28.7662 |
| 5 | 34,492,600 | 28.9917 |

### Summary

| Metric | Median | Observed range |
|---|---:|---:|
| Throughput | 34,492,600 events/s | 33,343,200–35,479,000 events/s |
| Average cost | 28.9917 ns/event | 28.1857–29.9911 ns/event |

## Percentile Latency

Each trial timed 100,000 events individually.

### Raw trials

| Trial | p50 | p95 | p99 | Maximum |
|---:|---:|---:|---:|---:|
| 1 | 42 ns | 83 ns | 84 ns | 15,250 ns |
| 2 | 42 ns | 42 ns | 84 ns | 25,792 ns |
| 3 | 42 ns | 83 ns | 84 ns | 30,666 ns |
| 4 | 42 ns | 83 ns | 84 ns | 15,917 ns |
| 5 | 42 ns | 42 ns | 84 ns | 22,416 ns |

### Summary

| Metric | Median | Observed range |
|---|---:|---:|
| p50 | 42 ns | 42–42 ns |
| p95 | 83 ns | 42–83 ns |
| p99 | 84 ns | 84–84 ns |
| Maximum | 22,416 ns | 15,250–30,666 ns |

The maximum is the single slowest individually timed event in each trial. It is
especially sensitive to operating-system scheduling, interrupts, and other
machine activity. It is not equivalent to the steady-state percentile latency
and should not be interpreted as a representative per-event cost.

## Cancellation Stress

Each order count was measured in every benchmark trial. All orders rested at
one price level and were cancelled front-to-back.

### Raw trials

| Resting orders | Trial 1 cancel ns/op | Trial 2 cancel ns/op | Trial 3 cancel ns/op | Trial 4 cancel ns/op | Trial 5 cancel ns/op |
|---:|---:|---:|---:|---:|---:|
| 5,000 | 14.7918 | 15.9166 | 14.2832 | 16.8416 | 17.4334 |
| 10,000 | 14.1916 | 15.3667 | 14.3500 | 13.9042 | 15.3208 |
| 20,000 | 14.2521 | 15.8729 | 15.1062 | 16.6667 | 15.5292 |
| 40,000 | 18.0375 | 15.3969 | 17.6187 | 27.0167 | 20.1448 |
| 80,000 | 15.0693 | 13.4380 | 14.8094 | 15.7740 | 16.1000 |

### Summary

| Resting orders | Median cancel cost | Observed range |
|---:|---:|---:|
| 5,000 | 15.9166 ns/cancel | 14.2832–17.4334 ns/cancel |
| 10,000 | 14.3500 ns/cancel | 13.9042–15.3667 ns/cancel |
| 20,000 | 15.5292 ns/cancel | 14.2521–16.6667 ns/cancel |
| 40,000 | 18.0375 ns/cancel | 15.3969–27.0167 ns/cancel |
| 80,000 | 15.0693 ns/cancel | 13.4380–16.1000 ns/cancel |

The approximately flat medians from 5,000 through 80,000 resting orders are
consistent with the slot pool's O(1) intrusive-list unlink, rather than the
linear element shifting of the former vector-backed price level.

This does not mean the complete cancellation API is mathematically
constant-time in every component. It still performs an order-identifier lookup
and a price-level map lookup, and those lookup costs remain part of the
operation.

## Interpretation

The current five-run medians are:

- aggregate throughput of 33,870,600 events/s
- aggregate average cost of 29.5241 ns/event
- deep-book throughput of 34,492,600 events/s
- deep-book average cost of 28.9917 ns/event
- per-event latency of 42 ns p50, 83 ns p95, and 84 ns p99
- cancellation medians between approximately 14 and 18 ns across 5,000 to
  80,000 resting orders

These values establish a current local baseline for detecting regressions and
guiding future measurement. They do not establish production exchange
performance, worst-case real-market latency, or portability of the absolute
numbers to another machine.
