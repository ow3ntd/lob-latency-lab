#!/usr/bin/env bash
set -euo pipefail

echo "Configuring Release build..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

echo "Building..."
cmake --build build

echo "Running order book tests..."
./build/test_order_book

echo "Running market data replay tests..."
./build/test_market_data_replay

echo "Running benchmark..."
./build/bench_order_book