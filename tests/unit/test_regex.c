/**
 * @file test_regex.c
 * @brief Unit tests for Layer 3 Pike VM regular expression engine
 */

#include "cobalt/algorithm/regex.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/runtime/error.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mock allocator for regex allocation testing */
static char   mock_re_buf[32768];
static size_t mock_re_off         = 0;
static int    mock_re_alloc_count = 0;
static int    mock_re_free_count  = 0;

static void *mock_re_alloc(cobalt_allocator_t *self, size_t size)
{
    (void)self;
    mock_re_alloc_count++;
    /* 8-byte align size */
    size_t aligned = (size + 7U) & ~7U;
    if (mock_re_off + aligned > sizeof(mock_re_buf)) {
        return NULL;
    }
    void *ptr = &mock_re_buf[mock_re_off];
    mock_re_off += aligned;
    return ptr;
}

static void mock_re_free(cobalt_allocator_t *self, void *ptr)
{
    (void)self;
    (void)ptr;
    mock_re_free_count++;
}

static void *mock_re_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
{
    void *new_ptr = mock_re_alloc(self, new_size);
    if (!new_ptr) {
        return NULL;
    }
    if (ptr) {
        memcpy(new_ptr, ptr, new_size);
    }
    return new_ptr;
}

static cobalt_allocator_t mock_re_alloc_inst = {
    .alloc   = mock_re_alloc,
    .free    = mock_re_free,
    .realloc = mock_re_realloc,
};

static void mock_re_reset(void)
{
    mock_re_off         = 0;
    mock_re_alloc_count = 0;
    mock_re_free_count  = 0;
}

static void test_regex_compile_valid(void)
{
    printf("Testing regex valid pattern compilation...\n");

    const char *patterns[] = {
        "abc",
        "hello_world123",
        "a|b|c",
        "[a-z0-9_]",
        "[^a-z]",
        "\\d+",
        "\\w*",
        "\\s?",
        "a*",
        "a+",
        "a?",
        "a{2,5}",
        "a{3}",
        "a{2,}",
        "a*?",
        "a+?",
        "a??",
        "a{2,5}?",
        "(abc)",
        "(?:abc)",
        "(a)(b)\\1",
        "^abc$",
        "\\bword\\b",
        "\\Bword\\B",
        "[a-z]+@[a-z0-9-]+\\.[a-z]{2,6}",
        "",
        "a|",
        "|b",
        "((a)(b(c)))",
    };

    size_t count = sizeof(patterns) / sizeof(patterns[0]);
    for (size_t i = 0; i < count; i++) {
        cobalt_regex_t *re = cobalt_regex_compile(patterns[i], COBALT_REGEX_DEFAULT);
        TEST_ASSERT(re != NULL);
        cobalt_regex_destroy(re);
    }

    /* Test compilation with flags */
    cobalt_regex_t *re_ci = cobalt_regex_compile("abc", COBALT_REGEX_CASE_INSENSITIVE);
    TEST_ASSERT(re_ci != NULL);
    cobalt_regex_destroy(re_ci);

    cobalt_regex_t *re_dot = cobalt_regex_compile(".*", COBALT_REGEX_DOTALL);
    TEST_ASSERT(re_dot != NULL);
    cobalt_regex_destroy(re_dot);

    cobalt_regex_t *re_multi = cobalt_regex_compile("^abc$", COBALT_REGEX_MULTILINE);
    TEST_ASSERT(re_multi != NULL);
    cobalt_regex_destroy(re_multi);

    printf("  valid pattern compilation: OK\n");
}

static void test_regex_compile_invalid(void)
{
    printf("Testing regex syntax error detection...\n");

    const char *invalid_patterns[] = {
        "(abc",     /* Unmatched open parenthesis */
        "(a|b",     /* Unmatched open parenthesis in alternation */
        "abc)",     /* Unmatched closing parenthesis */
        "[a-z",     /* Unclosed bracket */
        "[abc",     /* Unclosed bracket */
        "[z-a]",    /* Invalid inverted range */
        "a{5,2}",   /* Invalid quantifier: min > max */
        "a{150}",   /* Quantifier exceeds 128 limit */
        "a{1,150}", /* Quantifier exceeds 128 limit */
        "*abc",     /* Dangling quantifier */
        "+abc",     /* Dangling quantifier */
        "?abc",     /* Dangling quantifier */
        "{2}abc",   /* Dangling quantifier */
        "\\1",      /* Undefined backreference (no capture groups) */
        "(a)\\2",   /* Undefined backreference (group 2 not defined) */
        "abc\\",    /* Trailing escape */
        "a|*b",     /* Dangling quantifier after pipe */
        "(*a)",     /* Dangling quantifier in group */
    };

    size_t count = sizeof(invalid_patterns) / sizeof(invalid_patterns[0]);
    for (size_t i = 0; i < count; i++) {
        cobalt_error_set(NULL, COBALT_SUCCESS);
        cobalt_regex_t *re = cobalt_regex_compile(invalid_patterns[i], COBALT_REGEX_DEFAULT);
        TEST_ASSERT(re == NULL);
        TEST_EQUAL(cobalt_error_get_current(), COBALT_ERROR_INVALID_ARGUMENT);
    }

    /* Test NULL pattern */
    cobalt_error_set(NULL, COBALT_SUCCESS);
    cobalt_regex_t *re_null = cobalt_regex_compile(NULL, COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re_null == NULL);
    TEST_EQUAL(cobalt_error_get_current(), COBALT_ERROR_INVALID_ARGUMENT);

    printf("  syntax error detection: OK\n");
}

static void test_regex_capture_counts(void)
{
    printf("Testing regex capture group counts...\n");

    struct {
        const char *pattern;
        size_t      expected_captures;
    } cases[] = {
        {"abc", 0},
        {"(abc)", 1},
        {"(a)(b)(c)", 3},
        {"(?:a)(b)", 1},
        {"(?:a)(?:b)", 0},
        {"((a)(b))", 3},
        {"((a)(b(c)))", 4},
        {"(?:\\b)(\\d+)", 1},
    };

    size_t count = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0; i < count; i++) {
        cobalt_regex_t *re = cobalt_regex_compile(cases[i].pattern, COBALT_REGEX_DEFAULT);
        TEST_ASSERT(re != NULL);
        TEST_EQUAL(cobalt_regex_capture_count(re), cases[i].expected_captures);
        cobalt_regex_destroy(re);
    }

    /* Destroying NULL should be safe */
    cobalt_regex_destroy(NULL);
    TEST_EQUAL(cobalt_regex_capture_count(NULL), 0U);

    printf("  capture group counts: OK\n");
}

static void test_regex_custom_allocator(void)
{
    printf("Testing regex custom allocator injection...\n");

    mock_re_reset();
    cobalt_allocator_t *alloc = &mock_re_alloc_inst;

    cobalt_regex_t *re = cobalt_regex_compile_with_alloc("(abc)+|(\\d{2,4})", 0, alloc);
    TEST_ASSERT(re != NULL);
    TEST_ASSERT(mock_re_alloc_count > 0);
    TEST_EQUAL(cobalt_regex_capture_count(re), 2U);

    int allocs_before_destroy = mock_re_alloc_count;
    cobalt_regex_destroy(re);
    TEST_ASSERT(mock_re_free_count > 0);
    TEST_EQUAL(mock_re_alloc_count, allocs_before_destroy);

    printf("  custom allocator injection: OK\n");
}

void test_regex(void)
{
    printf("=== Regex Engine Tests ===\n");
    test_regex_compile_valid();
    test_regex_compile_invalid();
    test_regex_capture_counts();
    test_regex_custom_allocator();
    printf("All regex compiler unit tests passed!\n");
}
