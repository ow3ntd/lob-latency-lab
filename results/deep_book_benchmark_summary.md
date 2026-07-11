# Deep Order Book Benchmark Summary

## Benchmark Goal

This benchmark measures order book performance under a deeper synthetic workload.

Unlike the baseline benchmark, which uses a small bounded 4-event cycle, this benchmark keeps many price levels and resting orders active so the order book remains deeper throughout the timed run.

## Benchmark Design

The benchmark generates deterministic synthetic events in memory before timing begins.

The workload includes:

- many bid price levels
- many ask price levels
- resting liquidity on both sides
- add events
- cancel events
- crossing orders that generate trades

The goal is to stress order book behavior under a larger active book state.

## Run Command

```bash
./build/bench_deep_order_book 1000000
```

## Results (2026-07-11, slot-pool implementation)

Machine: MacBook Pro  
Compiler: Apple clang++  
Optimization flags: `-O3 -DNDEBUG`  
Events per run: 1,000,000

| Run | Events Processed | Trades Generated | Elapsed Time | Events / Second | Avg ns / Event |
|---:|---:|---:|---:|---:|---:|
| 1 | 1,000,000 | 250,000 | 0.0536278 seconds | 18,647,000 | 53.6278 |
| 2 | 1,000,000 | 250,000 | 0.0368480 seconds | 27,138,500 | 36.8480 |
| 3 | 1,000,000 | 250,000 | 0.0380741 seconds | 26,264,600 | 38.0741 |
| **Average** | **1,000,000** | **250,000** | **0.0428500 seconds** | **24,016,700** | **42.8500** |

## Comparison with the Pre-Optimization Implementation

The same benchmark measured on the earlier vector-backed implementation, before the 2026-07-07 slot-pool cancellation redesign:

| Implementation | Events / Second | Avg ns / Event |
|---|---:|---:|
| Old vector-backed cancel | 177,787 | 5,624.72 |
| Slot-pool cancel (current) | 24,016,700 | 42.85 |

That is roughly a 131x throughput improvement on this workload. The deep book was the case where the old O(n) cancellation scan dominated: with many resting orders per level, each cancel had to shift elements within the level's vector. The slot-pool design unlinks the cancelled node in place, so per-event cost no longer grows with book depth.

Deep-book cost (~42.9 ns/event) is now roughly at parity with the shallow 4-event-cycle benchmark (~40.8 ns/event), which indicates cancellation was the dominant deep-book cost rather than matching or price-level lookup.

## Result Summary

Across three runs on the slot-pool implementation, the benchmark processed approximately **24.0 million events per second**, with an average processing cost of about **42.9 nanoseconds per event**.

## Measurement Limitations

- The first run after a build is consistently slower (53.6 ns vs ~37-38 ns on subsequent runs), likely due to cold instruction and data caches; all runs are reported rather than discarding the warmup.
- The workload is deterministic and synthetic, so it is useful for regression tracking but not a full model of real market data.
- This benchmark measures aggregate throughput, not per-event percentile latency.
- Results vary by CPU, compiler, optimization flags, thermal state, and background system load.
