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