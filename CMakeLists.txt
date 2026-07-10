cmake_minimum_required(VERSION 3.20)

project(lob_latency_lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_library(lob
    src/OrderBook.cpp
    src/MarketDataReplay.cpp
)

target_include_directories(lob PUBLIC include)

add_executable(lob_demo
    src/main.cpp
)

target_link_libraries(lob_demo PRIVATE lob)

add_executable(test_order_book
    tests/test_order_book.cpp
)

target_link_libraries(test_order_book PRIVATE lob)

add_executable(test_market_data_replay
    tests/test_market_data_replay.cpp
)

target_link_libraries(test_market_data_replay PRIVATE lob)

add_executable(bench_order_book
    benchmarks/bench_order_book.cpp
)

target_link_libraries(bench_order_book PRIVATE lob)

add_executable(bench_order_book_latency
    benchmarks/bench_order_book_latency.cpp
)

target_link_libraries(bench_order_book_latency PRIVATE lob)

add_executable(bench_deep_order_book
    benchmarks/bench_deep_order_book.cpp
)

target_link_libraries(bench_deep_order_book PRIVATE lob)

add_executable(bench_cancel_stress
    benchmarks/bench_cancel_stress.cpp
)

target_link_libraries(bench_cancel_stress PRIVATE lob)
add_executable(lob_lobster_demo
    src/lob_lobster_demo.cpp
)
target_link_libraries(lob_lobster_demo PRIVATE lob)
