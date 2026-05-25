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
