# LOBSTER Replay Summary

> **Status:** The numeric results below were recorded before in-place partial
> cancellation and visible-execution quantity reduction were implemented. They
> are retained as a historical parser-coverage run and must not be treated as
> current reconstruction-validation results.

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

- type 1 adds a visible resting order
- type 2 reduces the referenced order in place by the event quantity
- type 3 deletes the referenced order
- type 4 reduces the referenced resting quantity by the executed quantity
  without generating a new match
- type 5 counts a hidden execution without mutating the visible book
- type 6 counts a cross trade without mutating the visible book
- type 7 counts an auxiliary or halt event without mutating the visible book

Direction 1 maps to the buy side and -1 to the sell side. LOBSTER timestamps
(seconds after midnight, with sub-second decimals) are parsed into an integer
nanosecond count using fixed-point arithmetic, avoiding floating point in the
timestamp path.

## Run Command

```bash
./build/lob_lobster_demo data/AAPL_2012-06-21_34200000_57600000_message_10.csv
```

## Historical Results Before Corrected Quantity Semantics

Replaying the full AAPL 2012-06-21 sample (seconds 34200 to 57600, i.e. the
regular trading session) produced the following historical parser-coverage
record:

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

Parsing all 400,391 rows without error establishes coverage of the real LOBSTER
message schema, including numeric type codes, direction encoding, and
fixed-point timestamps. It does not establish that the reconstructed book
matches the paired LOBSTER order-book data.

The successful order-id lookup ratio is a bookkeeping diagnostic: it records
how often cancellation-type events found a referenced resting order under the
implementation used for that run. It is not proof of reconstruction
correctness.

At the end of the session the book shows a best bid of 5,775,600 and a best ask
of 5,775,700. LOBSTER prices are expressed in units of 1/10,000 of a dollar, so
this is a bid of \$577.56 and an ask of \$577.57. The non-crossed one-tick
spread is a basic top-of-book sanity check, not proof of exact reconstruction.

## Limitations

- No row-by-row comparison against the paired LOBSTER order-book file was
  performed.
- The published numeric run predates corrected type-2 partial-cancellation and
  type-4 visible-execution handling and must be regenerated.
- Hidden liquidity is not modeled.
- Visible executions now update the referenced resting quantity, but they do
  not produce a newly matched `Trade` object.
- The historical numbers come from one session file and establish parser
  coverage only; they do not establish exact book reconstruction.
