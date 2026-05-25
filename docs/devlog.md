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
