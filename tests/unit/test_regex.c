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

static void test_regex_literals(void)
{
    printf("Testing regex literal matching and exact matching...\n");

    cobalt_regex_t *re = cobalt_regex_compile("cat", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);

    TEST_EQUAL(cobalt_regex_search(re, "the cat sat", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "the dog sat", NULL, 0), 0);
    TEST_EQUAL(cobalt_regex_match_exact(re, "cat", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_match_exact(re, "the cat", NULL, 0), 0);
    TEST_EQUAL(cobalt_regex_match_exact(re, "cats", NULL, 0), 0);

    cobalt_regex_destroy(re);

    /* Empty pattern */
    re = cobalt_regex_compile("", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "abc", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_match_exact(re, "", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_match_exact(re, "a", NULL, 0), 0);
    cobalt_regex_destroy(re);

    /* Case insensitive matching */
    re = cobalt_regex_compile("HeLLo", COBALT_REGEX_CASE_INSENSITIVE);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "say hello there", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_match_exact(re, "HELLO", NULL, 0), 1);
    cobalt_regex_destroy(re);

    printf("  literal matching: OK\n");
}

static void test_regex_wildcard(void)
{
    printf("Testing regex wildcard and DOTALL...\n");

    cobalt_regex_t *re = cobalt_regex_compile("c.t", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "cat", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "cot", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "c\nt", NULL, 0), 0);
    cobalt_regex_destroy(re);

    /* With DOTALL */
    re = cobalt_regex_compile("c.t", COBALT_REGEX_DOTALL);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "c\nt", NULL, 0), 1);
    cobalt_regex_destroy(re);

    printf("  wildcard and DOTALL: OK\n");
}

static void test_regex_anchors(void)
{
    printf("Testing regex anchors (^, $, \\b, \\B) and MULTILINE...\n");

    cobalt_regex_t *re = cobalt_regex_compile("^cat", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "cat sat", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "the cat", NULL, 0), 0);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("cat$", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "the cat", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "cat sat", NULL, 0), 0);
    cobalt_regex_destroy(re);

    /* Word boundary */
    re = cobalt_regex_compile("\\bcat\\b", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "the cat sat", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "the scat sat", NULL, 0), 0);
    TEST_EQUAL(cobalt_regex_search(re, "the catch sat", NULL, 0), 0);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("\\Bcat\\b", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "the scat sat", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "the cat sat", NULL, 0), 0);
    cobalt_regex_destroy(re);

    /* Multiline */
    re = cobalt_regex_compile("^world", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "hello\nworld", NULL, 0), 0);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("^world", COBALT_REGEX_MULTILINE);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "hello\nworld", NULL, 0), 1);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("hello$", COBALT_REGEX_MULTILINE);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "hello\nworld", NULL, 0), 1);
    cobalt_regex_destroy(re);

    printf("  anchors: OK\n");
}

static void test_regex_classes(void)
{
    printf("Testing regex character classes and shorthands...\n");

    cobalt_regex_t *re = cobalt_regex_compile("[a-z]+", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "123hello456", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "123456", NULL, 0), 0);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("[^0-9]+", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "abc", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "123", NULL, 0), 0);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("\\d{3}", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "ab12cd", NULL, 0), 0);
    TEST_EQUAL(cobalt_regex_search(re, "ab123cd", NULL, 0), 1);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("\\w+", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "!@#$%", NULL, 0), 0);
    TEST_EQUAL(cobalt_regex_search(re, "!@#abc$%", NULL, 0), 1);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("\\s+", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "nospaces", NULL, 0), 0);
    TEST_EQUAL(cobalt_regex_search(re, "has spaces", NULL, 0), 1);
    cobalt_regex_destroy(re);

    printf("  character classes: OK\n");
}

static void test_regex_quantifiers(void)
{
    printf("Testing regex quantifiers (greedy and non-greedy)...\n");

    cobalt_regex_t *re = cobalt_regex_compile("ba*c", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "bc", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "bac", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "baaac", NULL, 0), 1);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("ba+c", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "bc", NULL, 0), 0);
    TEST_EQUAL(cobalt_regex_search(re, "bac", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "baaac", NULL, 0), 1);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("ba?c", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "bc", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "bac", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "baac", NULL, 0), 0);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("a{2,4}", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_match_exact(re, "a", NULL, 0), 0);
    TEST_EQUAL(cobalt_regex_match_exact(re, "aa", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_match_exact(re, "aaa", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_match_exact(re, "aaaa", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_match_exact(re, "aaaaa", NULL, 0), 0);
    cobalt_regex_destroy(re);

    /* Non-greedy */
    re = cobalt_regex_compile("a*?", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "aaaa", NULL, 0), 1);
    cobalt_regex_destroy(re);

    re = cobalt_regex_compile("a+?", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "aaaa", NULL, 0), 1);
    cobalt_regex_destroy(re);

    printf("  quantifiers: OK\n");
}

static void test_regex_alternation(void)
{
    printf("Testing regex alternation (cat|dog|fox)...\n");

    cobalt_regex_t *re = cobalt_regex_compile("cat|dog|fox", COBALT_REGEX_DEFAULT);
    TEST_ASSERT(re != NULL);
    TEST_EQUAL(cobalt_regex_search(re, "a dog barked", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "a fox ran", NULL, 0), 1);
    TEST_EQUAL(cobalt_regex_search(re, "a bird sang", NULL, 0), 0);
    cobalt_regex_destroy(re);

    printf("  alternation: OK\n");
}

static void test_regex_convenience(void)
{
    printf("Testing regex convenience API cobalt_regex_test...\n");

    TEST_EQUAL(cobalt_regex_test("hello.*world", "hello beautiful world"), 1);
    TEST_EQUAL(cobalt_regex_test("hello.*world", "goodbye world"), 0);
    TEST_EQUAL(cobalt_regex_test(NULL, "abc"), -1);
    TEST_EQUAL(cobalt_regex_test("(", "abc"), -1);

    printf("  convenience test API: OK\n");
}

void test_regex(void)
{
    printf("=== Regex Engine Tests ===\n");
    test_regex_compile_valid();
    test_regex_compile_invalid();
    test_regex_capture_counts();
    test_regex_custom_allocator();
    test_regex_literals();
    test_regex_wildcard();
    test_regex_anchors();
    test_regex_classes();
    test_regex_quantifiers();
    test_regex_alternation();
    test_regex_convenience();
    printf("All regex unit tests passed!\n");
}
