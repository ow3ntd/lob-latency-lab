# LOBSTER Transition Validation

## Scope

This report records a correctness-validation run of the local LOBSTER
transition validator against the official public AAPL 2012-06-21 Level-10
sample.

The paired dataset files were:

- `AAPL_2012-06-21_34200000_57600000_message_10.csv`
- `AAPL_2012-06-21_34200000_57600000_orderbook_10.csv`

The requested visible depth was 10. The licensed raw files are not committed
to this repository.

## Validation Model

The validator reads the message and order-book streams in strict lockstep.
Order-book row 1 is treated as an authoritative baseline because no preceding
snapshot is available. Starting at row 2, each transition is evaluated as:

```text
order-book row k-1
        + message row k
        -> order-book row k
```

This is local transition validation, not reconstruction from an empty book.
The regular-session message file does not include every order that was already
resting when row 1 was emitted, so it does not provide enough history to
reconstruct the authoritative row-1 snapshot from an empty `OrderBook`.

Each checked transition receives exactly one classification:

- **Matching:** the message explains the complete visible transition exactly.
- **Mismatching:** the observable paired snapshots differ from the transition
  implied by the message.
- **Unsupported:** the event type is deliberately not transition-validated.
- **Unverifiable:** all observable known levels are consistent, but omitted
  depth prevents an exact claim.

### Tail-refill classification

When a visible top-10 price level disappears, a previously hidden deeper level
may enter position 10. That new tail level's price and quantity were not
present in order-book row k-1. The validator therefore checks that every known
prefix level transitions correctly, but it does not claim to reconstruct or
validate the newly exposed tail.

Such a transition is conservatively classified as unverifiable rather than
matching. It is classified as mismatching if any observable known level or the
opposite side differs.

## Reproduction

Validation code commit:

```text
fbb46fc970dfd9aef786a8ea7c3490d2d272ede4
```

Environment:

| Field | Value |
|---|---|
| Compiler | Apple clang 21.0.0 (`clang-2100.0.123.102`) |
| Operating system | macOS 26.2, build 25C56, arm64 |
| Hardware | MacBook Pro (Mac16,1), Apple M4, 10 cores, 24 GB memory |

After placing both licensed files in `data/`, the exact command was:

```bash
./build-release/lob_lobster_transition_validate \
    data/AAPL_2012-06-21_34200000_57600000_message_10.csv \
    data/AAPL_2012-06-21_34200000_57600000_orderbook_10.csv \
    10
```

## Results

| Metric | Value |
|---|---:|
| Requested depth | 10 |
| Rows read | 400,391 |
| Baseline rows | 1 |
| Transitions checked | 400,390 |
| Matching transitions | 295,828 |
| Mismatching transitions | 0 |
| Unsupported transitions | 0 |
| Unverifiable transitions | 104,562 |
| Tail-refill transitions | 104,562 |
| Opposite-side changes | 0 |
| Missing event-price levels | 0 |
| Quantity underflows | 0 |
| Price mismatches | 0 |
| Quantity mismatches | 0 |
| Missing-level mismatches | 0 |
| Exit code | 0 |

| Outcome | Percentage |
|---|---:|
| Exact matching transitions | 73.88% |
| Unverifiable tail refills | 26.12% |
| Observable mismatches | 0.00% |

The unrounded calculated proportions were 73.884962% exact matching
transitions and 26.115038% unverifiable tail refills.

The conservative result is therefore:

- 295,828 exact transitions
- 104,562 conservative tail-refill classifications
- zero observable mismatches

## Interpretation and Limitations

This run demonstrates that every observable transition in the paired sample
was either explained exactly or reduced to a conservative tail-refill case in
which the known visible prefix remained consistent. No observable price,
quantity, missing-level, opposite-side, event-price, or underflow mismatch was
detected.

It does not claim:

- complete reconstruction from an empty book
- validation of unknown hidden depth
- validation of individual order IDs from aggregate snapshots
- production exchange equivalence

The validator compares aggregate visible price levels. Passing this check does
not reveal orders below the requested depth or prove the identity and FIFO
position of individual orders represented by an aggregate level.

This was a correctness-validation run, not a controlled performance
benchmark. No shell timing from the run is reported as a latency or throughput
measurement.
