#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace testfw {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failure_count() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back(TestCase{name, std::move(fn)});
    }
};

inline void record_failure(const char* file, int line, const std::string& msg) {
    ++failure_count();
    std::cerr << "[FAIL] " << file << ":" << line << ": " << msg << "\n";
}

}  // namespace testfw

#define TEST_CASE(name)                                                                  \
    static void test_fn_##name();                                                        \
    static ::testfw::Registrar test_reg_##name(#name, &test_fn_##name);                  \
    static void test_fn_##name()

#define CHECK(expr)                                                                      \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            ::testfw::record_failure(__FILE__, __LINE__, "CHECK(" #expr ") failed");     \
        }                                                                                \
    } while (false)

#define CHECK_EQ(actual, expected)                                                       \
    do {                                                                                 \
        const auto _a = (actual);                                                        \
        const auto _e = (expected);                                                      \
        if (!(_a == _e)) {                                                               \
            std::ostringstream _oss;                                                     \
            _oss << "CHECK_EQ(" #actual ", " #expected ") failed: got [" << _a           \
                 << "] expected [" << _e << "]";                                         \
            ::testfw::record_failure(__FILE__, __LINE__, _oss.str());                    \
        }                                                                                \
    } while (false)

#define CHECK_NEAR(actual, expected, eps)                                                \
    do {                                                                                 \
        const double _a = static_cast<double>(actual);                                   \
        const double _e = static_cast<double>(expected);                                 \
        const double _tol = static_cast<double>(eps);                                    \
        if (std::fabs(_a - _e) > _tol) {                                                 \
            std::ostringstream _oss;                                                     \
            _oss << "CHECK_NEAR(" #actual ", " #expected ") failed: got [" << _a         \
                 << "] expected [" << _e << "] (eps=" << _tol << ")";                    \
            ::testfw::record_failure(__FILE__, __LINE__, _oss.str());                    \
        }                                                                                \
    } while (false)
