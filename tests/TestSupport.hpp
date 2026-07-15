#pragma once

#include <cstdlib>
#include <iostream>

namespace test_support {

inline void check(bool result, const char* expression,
                  const char* file, int line) {
    if (!result) {
        std::cerr << "CHECK failed: " << expression
                  << " at " << file << ':' << line << '\n';
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace test_support

#define CHECK(expression)                                              \
    ::test_support::check(                                             \
        static_cast<bool>(expression), #expression, __FILE__, __LINE__ \
    )
