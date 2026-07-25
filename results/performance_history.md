# Performance History

This file tracks benchmark results over time with the implementation and
measurement context preserved. Results from different dates may use different
benchmark methodology, workloads, environments, or trial counts. They should
not be treated as a controlled before/after experiment unless those conditions
match.

## 2026-07-24 — Current Slot-Pool Baseline

Commit: `f9d629936d8cc04278434ac77f8b4baf5aed13bd`

Machine: MacBook Pro Mac16,1, Apple M4, 10 cores, 24 GB

Operating system: macOS 26.2, build 25C56, arm64

Compiler: Apple clang 21.0.0 (`clang-2100.0.123.102`)

Build: CMake Release

Methodology:

- all four registered CTest tests passed
- one discarded warm-up per benchmark executable
- five measured trials
- median and full observed range reported
- local machine otherwise uncontrolled
- deterministic synthetic microbenchmarks

### Current Summary

| Benchmark | Metric | Median | Observed range |
|---|---|---:|---:|
| Aggregate, 1,000,000 events | Throughput | 33,870,600 events/s | 32,501,100–35,025,900 events/s |
| Aggregate, 1,000,000 events | Average cost | 29.5241 ns/event | 28.5503–30.7682 ns/event |
| Deep book, 1,000,000 events | Throughput | 34,492,600 events/s | 33,343,200–35,479,000 events/s |
| Deep book, 1,000,000 events | Average cost | 28.9917 ns/event | 28.1857–29.9911 ns/event |
| Per-event latency, 100,000 events | p50 | 42 ns | 42–42 ns |
| Per-event latency, 100,000 events | p95 | 83 ns | 42–83 ns |
| Per-event latency, 100,000 events | p99 | 84 ns | 84–84 ns |
| Per-event latency, 100,000 events | Maximum | 22,416 ns | 15,250–30,666 ns |

### Current Cancellation Scaling

| Resting orders | Median cancel cost | Observed range |
|---:|---:|---:|
| 5,000 | 15.9166 ns/cancel | 14.2832–17.4334 ns/cancel |
| 10,000 | 14.3500 ns/cancel | 13.9042–15.3667 ns/cancel |
| 20,000 | 15.5292 ns/cancel | 14.2521–16.6667 ns/cancel |
| 40,000 | 18.0375 ns/cancel | 15.3969–27.0167 ns/cancel |
| 80,000 | 15.0693 ns/cancel | 13.4380–16.1000 ns/cancel |

The approximately flat cancellation medians across this range are consistent
with the slot pool's O(1) intrusive-list unlink. The complete cancellation API
also includes identifier and price-map lookups and is not claimed to be
constant-time in every component.

The aggregate and deep-book benchmarks use different deterministic workloads.
Their absolute values should not be interpreted as evidence that either
workload is universally faster.

Raw trials, commands, environment details, and limitations are recorded in
[`current_performance.md`](current_performance.md).

## 2026-06-23 — Baseline Order Book Performance

Commit: `7276a11`  
Machine: MacBook Pro  
Compiler: Apple clang++  
Flags: `-O3 -DNDEBUG`

### Throughput Benchmark

Command:

```bash
./build/bench_order_book 1000000
```

| Metric | Result |
|---|---:|
| Events processed | 1,000,000 |
| Trades generated | 250,000 |
| Average events/sec | 24,508,067 |
| Average ns/event | 40.8179 ns |

### Latency Benchmark

Command:

```bash
./build/bench_order_book_latency 100000
```

| Metric | Result |
|---|---:|
| Events processed | 100,000 |
| Trades generated | 25,000 |
| p50 latency | 42 ns |
| p95 latency | ~84 ns |
| p99 latency | 84 ns |
| Worst max latency observed | 30,083 ns |

### Notes

- Throughput and latency are measured separately.
- The latency benchmark times each event individually, so it has extra measurement overhead.
- The workload is deterministic and synthetic.
- These numbers are intended as a baseline for future optimization work.
- This older baseline predates the slot-pool implementation and used a
  different reporting methodology. It is retained as historical context, not
  as a controlled comparison with the 2026-07-24 measurements.
