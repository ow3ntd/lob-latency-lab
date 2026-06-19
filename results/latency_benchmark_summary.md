# Latency Benchmark Summary

## Benchmark Goal

This benchmark measures per-event order book latency using percentile metrics.

It is separate from the throughput benchmark because timing every individual event adds measurement overhead.

## Benchmark Design

The benchmark generates deterministic synthetic events before timing begins.

Each event is timed individually using `std::chrono::steady_clock`, and the per-event latency is stored in nanoseconds.

After the run, latencies are sorted and percentile metrics are reported.

## Run Command

```bash
./build/bench_order_book_latency 100000
```

## Results

Events per run: 100,000  
Trades generated per run: 25,000  

| Run | Events Processed | Trades Generated | p50 Latency | p95 Latency | p99 Latency | Max Latency |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 100,000 | 25,000 | 42 ns | 83 ns | 84 ns | 16,875 ns |
| 2 | 100,000 | 25,000 | 42 ns | 84 ns | 84 ns | 16,250 ns |
| 3 | 100,000 | 25,000 | 42 ns | 84 ns | 84 ns | 30,083 ns |

## Result Summary

Across three runs, the benchmark reported:

- p50 latency: approximately 42 ns
- p95 latency: approximately 84 ns
- p99 latency: approximately 84 ns
- worst observed max latency: 30,083 ns

## Measurement Limitations

- This benchmark times each event individually, which adds overhead.
- The benchmark is useful for comparing versions of the code, but the absolute nanosecond values should be interpreted carefully.
- The workload is deterministic and synthetic, not a full model of real market data.
- Max latency can be affected by CPU scheduling, background processes, and thermal state.
- Throughput should be evaluated separately using `bench_order_book.cpp`.
