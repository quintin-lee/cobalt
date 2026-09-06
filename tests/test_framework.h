/**
 * @file test_framework.h
 * @Simple test framework for Cobalt unit tests
 * Provides basic macros and registration mechanism.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

/* Test result tracking */
typedef struct {
    int passed;
    int failed;
    int total;
} TestResults;

static TestResults g_test_results __attribute__((unused)) = {0, 0, 0};

/* Simple assertion macro - aborts on failure in this simple framework */
#define TEST_ASSERT(cond)                                                                          \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__);         \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/* Simple equality check with message */
#define TEST_EQUAL(actual, expected)                                                               \
    do {                                                                                           \
        g_test_results.total++;                                                                    \
        if ((actual) == (expected)) {                                                              \
            g_test_results.passed++;                                                               \
        } else {                                                                                   \
            fprintf(stderr,                                                                        \
                    "Assertion failed: %s == %s at %s:%d\n",                                       \
                    #actual,                                                                       \
                    #expected,                                                                     \
                    __FILE__,                                                                      \
                    __LINE__);                                                                     \
            g_test_results.failed++;                                                               \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/* Register a test function */
typedef void (*test_func_t)(void);

#define TEST_REGISTER(name)                                                                        \
    static void name(void);                                                                        \
    static void __reg_##name(void);                                                                \
    static void __reg_##name(void)                                                                 \
    { /* registration happens via C init */                                                        \
    }                                                                                              \
    void name(void)                                                                                \
    {                                                                                              \
        __register_##name();                                                                       \
    } /* placeholder */

/*
 * Auto-registration macro: use TEST_AUTO_REGISTER(name) after defining
 * the test function to register it without modifying test_runner.c.
 */
#define TEST_AUTO_REGISTER(name)                                                                   \
    static void *__test_entry_##name __attribute__((used, section(".test_registry"))) =            \
        (void *)name;                                                                              \
    static const char *__test_name_##name __attribute__((used, section(".test_names"))) = #name;

/* Declare a test function - the actual registration will be done in runner */

#endif /* TEST_FRAMEWORK_H */
