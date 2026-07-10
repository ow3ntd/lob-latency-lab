# LOBSTER Replay Summary

## Goal

This document records the result of replaying a real LOBSTER message file
through the order book, and explains why the data file itself is not committed
to this repository.

The goal is to exercise the engine on real NASDAQ event sequences and data
shapes rather than only synthetic events. It is not to reproduce NASDAQ's exact
trade prints: LOBSTER data is already reconstructed, so executions and hidden
liquidity are resolved in the source file rather than re-derived by this engine.

## Data Source and Licensing

The sample used here is the LOBSTER academic sample file for AAPL on
2012-06-21, level 10 (`AAPL_2012-06-21_34200000_57600000_message_10.csv`).

LOBSTER data is derived from NASDAQ Historical TotalView-ITCH under an academic
waiver granted by NASDAQ OMX. Redistribution of the raw data is not permitted
under those terms, so the message file is intentionally excluded from version
control via `.gitignore`. Only this results summary and a small synthetic
sample (`data/sample_lobster_message.csv`, generated locally in the same
format) are committed.

To reproduce the numbers below, download the free sample from
`lobsterdata.com`, place the message file in `data/`, and run the demo against
it (see Run Command).

## Reader Behavior

The LOBSTER reader (`replay_lobster_data` in `src/MarketDataReplay.cpp`) maps
LOBSTER message-type codes onto the order book as follows:

- type 1 (new limit order) is added to the book
- type 2 (partial cancellation) removes the order and re-adds the reduced size
- type 3 (deletion) cancels the order
- type 4 (visible execution) is counted but not re-matched, since the
  execution is already resolved in the source data
- type 5 (hidden execution) is counted and skipped, as hidden liquidity is not
  part of the visible book
- type 7 (auxiliary, e.g. trading halt) is counted and skipped

Direction 1 maps to the buy side and -1 to the sell side. LOBSTER timestamps
(seconds after midnight, with sub-second decimals) are parsed into an integer
nanosecond count using fixed-point arithmetic, avoiding floating point in the
timestamp path.

## Run Command

```bash
./build/lob_lobster_demo data/AAPL_2012-06-21_34200000_57600000_message_10.csv
```

## Results

Replaying the full AAPL 2012-06-21 sample (seconds 34200 to 57600, i.e. the
regular trading session):

| Metric | Value |
|---|---:|
| Events processed | 400,391 |
| New limit orders (type 1) | 191,015 |
| Partial cancels (type 2) | 3,260 |
| Deletions (type 3) | 171,126 |
| Visible executions (type 4, counted) | 23,658 |
| Hidden executions (type 5, skipped) | 11,332 |
| Auxiliary events (type 7, skipped) | 0 |
| Successful cancels | 152,223 |
| Executed quantity (sum of type-4 sizes) | 1,845,964 |
| Resting orders at end | 3,441 |
| Best bid at end | 5,775,600 |
| Best ask at end | 5,775,700 |

## Interpretation

The reader parsed all 400,391 events without error, which confirms the parser
handles the real LOBSTER schema (numeric type codes, direction encoding, and
fixed-point timestamps) and not just the synthetic sample.

The high ratio of successful cancels to deletion and partial-cancel events
(152,223 out of 174,386) indicates that order-id bookkeeping stays consistent
across hundreds of thousands of events: most deletions found their target
resting order and removed it. A parser or bookkeeping bug would instead produce
a large number of failed cancels against unknown order ids.

At the end of the session the book shows a best bid of 5,775,600 and a best ask
of 5,775,700. LOBSTER prices are expressed in units of 1/10,000 of a dollar, so
this is a bid of \$577.56 and an ask of \$577.57, a one-tick spread, which is
consistent with a liquid name and indicates the reconstructed book is coherent
rather than crossed or nonsensical.

## Limitations

- This engine does not reproduce LOBSTER's executions; type-4 events are
  counted, not re-matched, so the trade sequence is not re-derived here.
- Hidden orders (type 5) are outside the visible book and are not modeled.
- The numbers above come from a single machine and a single session file; they
  characterize correctness and event coverage, not latency.
