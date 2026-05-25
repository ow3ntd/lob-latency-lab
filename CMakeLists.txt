cmake_minimum_required(VERSION 3.20)

project(lob_latency_lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(lob
    src/OrderBook.cpp
    src/MatchingEngine.cpp
)

target_include_directories(lob PUBLIC include)

add_executable(lob_demo
    src/main.cpp
)

target_link_libraries(lob_demo PRIVATE lob)
