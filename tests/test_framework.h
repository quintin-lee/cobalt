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

/* Unnamed, auto-deleted scratch file for tests.
 * C89 tmpfile(): no path construction, no name collision, removed on close.
 * Portable across POSIX and Windows (MinGW). Returns NULL on failure.
 *
 * Tests MUST NOT hardcode absolute paths such as "/tmp/...": they silently
 * fail on Windows, where such directories do not exist. Use this helper
 * whenever a test needs a readable-back stream. */
static FILE *cobalt_test_tmpfile(void) __attribute__((unused));
static FILE *cobalt_test_tmpfile(void)
{
    return tmpfile();
}

#endif /* TEST_FRAMEWORK_H */
