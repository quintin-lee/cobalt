/**
 * @file test_framework.h
 * @Simple test framework for Cobalt unit tests
 * Provides basic macros and registration mechanism.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include "cobalt/memory/allocator.h"
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

/* -------------------------------------------------------------------------- */
/* Counting allocator — delegates to the system allocator, records traffic     */
/* -------------------------------------------------------------------------- */
/* Proves that an injected allocator actually receives allocations and that   */
/* every allocation is released. Wrap with cobalt_test_counting_allocator()    */
/* and query cobalt_test_count_allocs / cobalt_test_count_frees.               */

static cobalt_allocator_t *cobalt_test_count_sys __attribute__((unused));
static int                 cobalt_test_count_allocs __attribute__((unused));
static int                 cobalt_test_count_frees __attribute__((unused));

static void *cobalt_test_counting_alloc(cobalt_allocator_t *self, size_t size)
    __attribute__((unused));
static void *cobalt_test_counting_alloc(cobalt_allocator_t *self, size_t size)
{
    (void)self;
    cobalt_test_count_allocs++;
    return cobalt_test_count_sys->alloc(cobalt_test_count_sys, size);
}

static void cobalt_test_counting_free(cobalt_allocator_t *self, void *ptr) __attribute__((unused));
static void cobalt_test_counting_free(cobalt_allocator_t *self, void *ptr)
{
    (void)self;
    cobalt_test_count_frees++;
    cobalt_test_count_sys->free(cobalt_test_count_sys, ptr);
}

static void *cobalt_test_counting_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
    __attribute__((unused));
static void *cobalt_test_counting_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
{
    (void)self;
    return cobalt_test_count_sys->realloc(cobalt_test_count_sys, ptr, new_size);
}

static cobalt_allocator_t cobalt_test_counting_v
    __attribute__((unused)) = {.alloc   = cobalt_test_counting_alloc,
                               .free    = cobalt_test_counting_free,
                               .realloc = cobalt_test_counting_realloc};

static void cobalt_test_counting_init(void) __attribute__((unused));
static void cobalt_test_counting_init(void)
{
    cobalt_test_count_sys    = cobalt_allocator_get_system();
    cobalt_test_count_allocs = 0;
    cobalt_test_count_frees  = 0;
}

#endif /* TEST_FRAMEWORK_H */
