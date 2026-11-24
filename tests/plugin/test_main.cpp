#include "test_framework.hpp"

#include <iostream>

TEST_CASE(test_harness_smoke) {
    CHECK(1 + 1 == 2);
}

int main() {
    int passed = 0;
    int failed_tests = 0;

    for (const auto& tc : testfw::registry()) {
        const int before = testfw::failure_count();
        try {
            tc.fn();
        } catch (const std::exception& ex) {
            testfw::record_failure(__FILE__, __LINE__, std::string("Unhandled exception: ") + ex.what());
        } catch (...) {
            testfw::record_failure(__FILE__, __LINE__, "Unhandled unknown exception");
        }
        if (testfw::failure_count() == before) {
            ++passed;
            std::cout << "[PASS] " << tc.name << "\n";
        } else {
            ++failed_tests;
            std::cerr << "[FAIL] " << tc.name << "\n";
        }
    }

    std::cout << "Summary: " << passed << " passed, " << failed_tests << " failed.\n";
    return failed_tests == 0 ? 0 : 1;
}
