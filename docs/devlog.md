# Development Log

## 2026-05-24

Created the initial repository structure for `lob-latency-lab`.

I decided to start with a design document before writing implementation code so that the project shows engineering planning instead of looking like a one-shot generated project.

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
