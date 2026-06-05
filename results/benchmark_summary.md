# Benchmark Summary

## Benchmark Goal

This benchmark measures aggregate throughput for the C++ order book and matching engine.

The goal is to measure order-processing performance, not CSV parsing, file input, or console output.

## Benchmark Design

The benchmark generates deterministic synthetic order events in memory before timing begins.

The timed loop processes a repeated 4-event cycle:

1. Add resting sell order
2. Add resting buy order
3. Add crossing buy or sell order that generates a trade
4. Cancel the remaining non-crossing order

This keeps the order book size bounded while exercising:

- add order logic
- cancel order logic
- buy-side matching
- sell-side matching
- trade generation

## Build Command

```bash
clang++ -std=c++20 -O3 -DNDEBUG -Iinclude \
  benchmarks/bench_order_book.cpp src/OrderBook.cpp src/MatchingEngine.cpp \
  -o build/bench_order_book
```

## Run Command

```bash
./build/bench_order_book 1000000
```

## Results

Machine: MacBook Pro  
Compiler: Apple clang++  
Optimization flags: `-O3 -DNDEBUG`  
Events per run: 1,000,000

| Run | Events Processed | Trades Generated | Elapsed Time | Events / Second | Avg ns / Event |
|---:|---:|---:|---:|---:|---:|
| 1 | 1,000,000 | 250,000 | 0.0409904 seconds | 24,395,900 | 40.9904 |
| 2 | 1,000,000 | 250,000 | 0.0397868 seconds | 25,134,000 | 39.7867 |
| 3 | 1,000,000 | 250,000 | 0.0416765 seconds | 23,994,300 | 41.6765 |
| **Average** | **1,000,000** | **250,000** | **0.0408179 seconds** | **24,508,067** | **40.8179** |

## Result Summary

Across three runs, the benchmark processed approximately **24.5 million events per second**, with an average processing cost of about **40.8 nanoseconds per event**.

## Measurement Limitations

- This benchmark measures aggregate throughput, not p50/p95/p99 per-event latency.
- Per-event latency should be measured separately because timing every event adds overhead.
- The workload is deterministic and synthetic, so it is useful for regression tracking but not a full model of real market data.
- Current cancellation logic scans price levels after finding the order side, so cancellation cost depends on book shape.
- Trade vector allocation inside `add_order` is included in the measured cost.
- Results vary by CPU, compiler, optimization flags, thermal state, and background system load.

## Next Steps

- Add a separate latency benchmark that reports p50, p95, and p99.
- Compare the baseline implementation against optimized data structures.
- Add a larger synthetic workload with deeper book levels.