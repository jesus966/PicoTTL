// PicoTTL - host test framework
//
// Minimal, dependency-free test harness (the "no external libraries"
// principle applies to the test suite too). Tests self-register through
// static TestCase instances - no heap, no exceptions.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdio>

namespace picottl_test {

struct TestCase {
    const char* name;
    void (*function)();
    TestCase* next;

    TestCase(const char* testName, void (*testFunction)());
};

/// Registry head (intrusive singly-linked list of static instances).
TestCase*& registry();

/// Set by CHECK failures inside the currently running test.
extern bool currentTestFailed;

/// Runs every registered test; returns the number of failed tests.
int runAllTests();

void reportFailure(const char* file, int line, const char* expression);
void reportFailureEq(const char* file, int line, const char* expression,
                     long long actual, long long expected);

} // namespace picottl_test

#define PICOTTL_TEST(name)                                                  \
    static void picottlTest_##name();                                       \
    static ::picottl_test::TestCase picottlTestCase_##name(                 \
        #name, &picottlTest_##name);                                        \
    static void picottlTest_##name()

#define CHECK(expression)                                                   \
    do {                                                                    \
        if (!(expression)) {                                                \
            ::picottl_test::reportFailure(__FILE__, __LINE__, #expression); \
        }                                                                   \
    } while (false)

#define CHECK_EQ(actual, expected)                                          \
    do {                                                                    \
        const auto a_ = (actual);                                           \
        const auto e_ = (expected);                                         \
        if (!(a_ == e_)) {                                                  \
            ::picottl_test::reportFailureEq(                                \
                __FILE__, __LINE__, #actual " == " #expected,               \
                static_cast<long long>(a_), static_cast<long long>(e_));    \
        }                                                                   \
    } while (false)
