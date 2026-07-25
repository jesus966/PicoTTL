// PicoTTL - host test framework runner
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "framework/test.hpp"

namespace picottl_test {

bool currentTestFailed = false;

TestCase*& registry() {
    static TestCase* head = nullptr;
    return head;
}

TestCase::TestCase(const char* testName, void (*testFunction)())
    : name(testName), function(testFunction), next(registry()) {
    registry() = this;
}

void reportFailure(const char* file, int line, const char* expression) {
    std::printf("    FAILED  %s:%d  CHECK(%s)\n", file, line, expression);
    currentTestFailed = true;
}

void reportFailureEq(const char* file, int line, const char* expression,
                     long long actual, long long expected) {
    std::printf("    FAILED  %s:%d  CHECK_EQ(%s)  actual=%lld expected=%lld\n",
                file, line, expression, actual, expected);
    currentTestFailed = true;
}

int runAllTests() {
    // The registry lists tests in reverse registration order; count and
    // run them as registered within each translation unit is fine - the
    // tests are independent by design.
    int total = 0;
    int failed = 0;
    for (TestCase* test = registry(); test != nullptr; test = test->next) {
        ++total;
        currentTestFailed = false;
        std::printf("RUN  %s\n", test->name);
        test->function();
        if (currentTestFailed) {
            ++failed;
        }
    }
    std::printf("\n%d test(s), %d failed\n", total, failed);
    return failed;
}

} // namespace picottl_test

int main() {
    return picottl_test::runAllTests();
}
