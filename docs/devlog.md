# Development Log

## 2026-05-24

Created the initial repository structure for `lob-latency-lab`.

I decided to start with a design document before writing implementation code to define correctness invariants, interfaces, and benchmark boundaries before implementation.

Initial scope:
- define basic order and trade types
- implement order book add/cancel/match logic
- add tests before optimization
- add benchmarks after correctness is working

Next step: define the core C++ domain types for orders, trades, prices, quantities, and sides.

## 2026-05-29 — Basic Matching Logic Added

Implemented basic crossing-order matching.

Current behavior:
- incoming buy orders match resting sell orders when buy price is greater than or equal to the best ask
- incoming sell orders match resting buy orders when sell price is less than or equal to the best bid
- trade events are generated with resting order ID, aggressive order ID, price, quantity, and timestamp
- partial fills are supported
- fully filled resting orders are removed from the book

I verified the demo with a buy order crossing a resting sell order. The program generated one trade at price 10060 with quantity 50.

Next step: add unit tests for add, cancel, partial fill, full fill, and non-crossing orders.

## 2026-05-29 — Correctness Tests Added

Added unit-style correctness tests for the order book.

Test coverage now includes:
- adding non-crossing buy and sell orders
- canceling an existing order
- canceling a missing order
- crossing buy partial fill
- crossing buy full fill with remainder
- crossing sell partial fill

At this stage, the tests used standard C++ `assert` statements so the project stayed simple and easy to compile without external dependencies.

Next step: add more edge-case tests, then add market data replay from CSV.

## 2026-06-03 — CSV Market Data Replay Added

Implemented CSV-based market data replay as a separate layer from the matching engine.

New behavior:
- reads ADD and CANCEL events from CSV input
- converts BUY and SELL text values into order book events
- replays events through the existing matching engine
- reports processed event counts, successful cancellations, generated trades, and total traded quantity
- supports in-memory replay tests using `std::istringstream`

I kept replay parsing separate from `OrderBook.cpp` so the matching engine remains independent of the input format and easier to test.

Verified sample replay:
- 4 events processed
- 3 add events
- 1 cancel event
- 1 generated trade
- total traded quantity of 50
- final remaining ask at price 10060

Next step: add latency and throughput benchmarking for replayed order events.

## 2026-06-04 — Benchmark Summary Added

Added a benchmark report for the deterministic order book benchmark.

The benchmark processes 1,000,000 synthetic events using a repeated add, match, and cancel pattern. The workload is generated in memory before timing starts so the benchmark measures order book processing rather than CSV parsing or file input.

Across three runs, the benchmark processed approximately 24.5 million events per second, with an average processing cost of about 40.8 nanoseconds per event.

I documented the benchmark design, build command, run command, results table, and measurement limitations in `results/benchmark_summary.md`.

Next step: add an architecture diagram and improve the README so the project is easier for recruiters to understand quickly.

## 2026-06-23 — Cancellation Optimization Experiment Reverted

Tested a cancellation-path optimization that replaced vector-backed price levels with list-backed price levels and direct order-location indexing.

The goal was to avoid scanning through price levels during cancellation.

Benchmark results showed the change made the current workload slower:

- baseline throughput: approximately 24.5M events/sec
- experimental throughput: approximately 15.6M events/sec
- baseline p50 latency: 42 ns
- experimental p50 latency: 83 ns
- baseline p99 latency: 84 ns
- experimental p99 latency: 125 ns

The likely cause is that `std::list` introduced worse cache locality and allocation/pointer-chasing overhead. In the current benchmark, the book stays small and bounded, so the original vector-backed scan is cheaper than the list-based direct-cancel approach.

I reverted the experiment and kept the faster baseline implementation.

Next step: create a deeper-book benchmark before attempting future cancellation optimizations.

## 2026-07-07 — O(1) Cancellation via Slot-Pool Linked List

Revisited cancellation, this time diagnosing why the earlier `std::list` attempt failed instead of concluding that direct-index cancellation was itself the problem.

The June 23 experiment was slower not because avoiding the scan was wrong, but because `std::list` allocates every node separately on the heap, which destroys cache locality on the matching hot path. The fix is to get the O(1) unlink without the per-node allocation.

I replaced the vector-backed price levels with an intrusive doubly-linked list stored in a single contiguous slot pool:
- each price level holds head, tail, and count indices instead of a `std::vector<Order>`
- orders live in one `std::vector<OrderNode>`, where each node stores its order plus prev and next as indices into that same vector rather than heap pointers
- a free-list of released slot indices lets cancelled slots be reused without ever calling `new`
- an `order_id -> slot index` hash map gives direct lookup, so cancellation unlinks the node in place with no scan and no element shifting

Because the nodes live in one contiguous array instead of scattered heap allocations, this keeps most of the cache locality that made the original vector fast, while making cancellation independent of book depth. This is the key difference from the reverted `std::list` version.

To actually measure the change I added a dedicated cancel-stress benchmark (`benchmarks/bench_cancel_stress.cpp`). The general-purpose throughput benchmark only makes one in four events a cancel and keeps the book small, so it dilutes the cancellation cost and could not isolate this. The stress benchmark rests N orders at one price level and cancels all of them front-to-back, which is the worst case for a position-shifting cancel.

Measured cancellation cost per operation, same benchmark on both implementations, `-O3 -DNDEBUG`:

| Orders | Old vector cancel (ns) | New slot-pool cancel (ns) |
|---:|---:|---:|
| 5,000 | ~1,960 | ~29 |
| 10,000 | ~3,953 | ~24 |
| 20,000 | ~7,923 | ~25 |
| 40,000 | ~15,858 | ~27 |
| 80,000 | ~46,703 | ~26 |

The old implementation's per-cancel cost roughly doubles each time the order count doubles, which is the expected O(n) signature. The new implementation stays flat at roughly 25-29 ns regardless of book depth. The speedup is not a fixed multiple; it grows with book size (about 68x at 5,000 orders up to roughly 1,760x at 80,000), which is what fixing an algorithmic complexity problem looks like rather than a constant-factor tweak.

One honest caveat on the claim: the slot unlink itself is O(1), but cancellation still does an `O(log P)` lookup into the price-level `std::map`, where P is the number of distinct price levels. The correct description is O(log P) map lookup plus O(1) unlink, not literally O(1) end to end.

I also added targeted tests for the cases the linked-list structure introduces: cancelling from the middle of a price level and confirming FIFO priority is preserved for the remaining orders, cancelling head and tail nodes, cancelling the only order so the price level is removed, and heavy add/cancel churn to confirm slot reuse does not corrupt the pool.

Next step: replay a real depth-of-book dataset (LOBSTER) instead of only synthetic events, and add a lock-free single-producer/single-consumer queue between market data ingestion and the order book.

## 2026-07-22 — Paired LOBSTER Transition Validation

The first paired-file validator attempted to reconstruct the regular-session
book from an empty `OrderBook`. Against the official AAPL Level-10 sample it
mismatched immediately: order-book row 1 already contained resting levels whose
submission events were not present in the regular-session message file. This
was a starting-state problem, not something that should be hidden by inventing
synthetic production orders.

I added a local transition validator that treats order-book row 1 as an
authoritative baseline. For every subsequent row k, it checks:

```text
order-book row k-1
        + message row k
        -> order-book row k
```

The validator classifies every checked transition as matching, mismatching,
unsupported, or unverifiable. It advances the authoritative baseline after
every syntactically valid row, so one mismatch or conservative classification
does not cascade into later comparisons.

The completed correctness-validation run used the official public AAPL
2012-06-21 Level-10 message and order-book files. It read 400,391 paired rows:

- 1 authoritative baseline row
- 400,390 transitions checked
- 295,828 exact matches (73.88%)
- 0 observable mismatches (0.00%)
- 0 unsupported transitions
- 104,562 unverifiable transitions (26.12%)

All 104,562 unverifiable transitions were conservative tail-refill cases. When
a visible top-10 level disappeared, the first nine known levels matched but a
previously hidden deeper level entered position 10. Because that new tail
level's prior price and quantity were not visible in row k-1, the validator
does not claim to have reconstructed it.

This result validates observable aggregate top-10 transitions for this sample.
It does not establish empty-start reconstruction, unknown hidden depth,
individual order IDs from aggregate snapshots, or production exchange
equivalence. The run was a correctness check; its shell runtime was not
recorded as a performance benchmark.

Full environment, command, counters, and limitations are recorded in
`results/lobster_transition_validation.md`.
