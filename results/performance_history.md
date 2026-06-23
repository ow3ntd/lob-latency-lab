# Performance History

This file tracks benchmark results over time so implementation changes can be compared against prior baselines.

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