/**
 * @file test_framework.h
 * @Simple test framework for Cobalt unit tests
 * Provides basic macros and registration mechanism.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Test result tracking */
typedef struct {
    int passed;
    int failed;
    int total;
} TestResults;

static TestResults g_test_results = {0, 0, 0};

/* Print test result */
static void test_report(const char *name, int passed) {
    g_test_results.total++;
    if (passed) {
        printf("[PASS] %s\n", name);
        g_test_results.passed++;
    } else {
        printf("[FAIL] %s\n", name);
        g_test_results.failed++;
    }
}

/* Simple assertion macro - aborts on failure in this simple framework */
#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

/* Simple equality check with message */
#define TEST_EQUAL(actual, expected) do { \
    if ((actual) != (expected)) { \
        fprintf(stderr, "Expected %d but got %d\n", (expected), (actual)); \
        g_test_results.failed++; g_test_results.total++; \
    } else { \
        g_test_results.passed++; g_test_results.total++; \
    } \
} while(0)

/* Register a test function */
typedef void (*test_func_t)(void);

#define TEST_REGISTER(name) static void name(void); static void __reg_##name(void); \
    static void __reg_##name(void) { /* registration happens via C init */ } \
    void name(void) { __register_##name(); } /* placeholder */

/* Declare a test function - the actual registration will be done in runner */
DECLARE_TEST(name);

#endif /* TEST_FRAMEWORK_H */
