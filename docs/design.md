# Design Notes: lob-latency-lab

## Goal

Build a simplified C++ limit order book and matching engine that supports add orders, cancel orders, partial fills, full fills, trade generation, market data replay, and latency benchmarking.

The purpose of this project is not to predict prices or build a trading strategy. The purpose is to practice the systems layer behind electronic trading infrastructure.

## Core Concepts

An order book stores buy and sell orders by price level.

Buy orders are called bids. Sell orders are called asks.

A trade happens when an incoming order crosses the opposite side of the book.

Example:

- Best ask: 100.60
- Incoming buy order: 100.60
- Result: trade occurs

## Price Representation

Prices will be stored as integer ticks instead of floating-point numbers.

Example:

- 100.50 dollars becomes 10050 ticks

This avoids floating-point rounding problems.

## First Version Scope

The first version will support:

- Add limit order
- Cancel order by order identifier
- Match aggressive buy order
- Match aggressive sell order
- Generate trade events
- Track best bid and best ask
- Replay order events from comma-separated value files
- Run correctness tests
- Run latency benchmarks

## Out of Scope for Version 1

- Real exchange data
- Network sockets
- Multi-threading
- Lock-free queues
- Strategy logic
- Graphical user interface

## Correctness Before Speed

The first version should be simple and correct. Optimization will come only after tests pass.

## Edge Cases to Test

- Add buy order to empty book
- Add sell order to empty book
- Incoming buy partially fills resting sell order
- Incoming buy fully fills resting sell order
- Incoming sell partially fills resting buy order
- Cancel existing order
- Cancel missing order
- Best bid updates after cancellation
- Best ask updates after fill

## Correctness Philosophy

For a matching engine, correctness matters before speed. The current tests focus on core order book behavior such as adding orders, canceling orders, matching crossing orders, and preserving remaining quantity after partial fills.

Planned correctness improvements include:

* FIFO matching for orders resting at the same price
* Best-price priority across multiple price levels
* Deterministic replay checks
* Cancel behavior for already-filled or missing orders
* Quantity invariants to prevent negative resting or traded quantity
* Basic volume conservation checks across submitted, filled, canceled, and resting quantity


## Current Matching Behavior

The order book now generates trades when incoming orders cross resting orders:

- an incoming buy order matches when its price is greater than or equal to the best ask
- an incoming sell order matches when its price is less than or equal to the best bid
- partially filled resting orders remain in the book
- fully filled resting orders are removed from the book


