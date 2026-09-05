/**
 * @file test_string.c
 * @brief Unit test for string utility functions.
 */

#include "cobalt/memory/allocator.h"
#include "cobalt/utils/string.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

void test_string_duplicate(void)
{
    printf("Testing cobalt_strdup...\n");

    char *copy = cobalt_strdup("hello");
    TEST_ASSERT(copy != NULL);
    TEST_ASSERT(strcmp(copy, "hello") == 0);
    free(copy);

    TEST_ASSERT(cobalt_strdup(NULL) == NULL);

    char *empty = cobalt_strdup("");
    TEST_ASSERT(empty != NULL);
    TEST_ASSERT(strcmp(empty, "") == 0);
    free(empty);

    printf("  strdup: OK\n");
}

void test_string_starts_with(void)
{
    printf("Testing cobalt_starts_with...\n");

    TEST_ASSERT(cobalt_starts_with("hello world", "hello") == 1);
    TEST_ASSERT(cobalt_starts_with("hello world", "world") == 0);
    TEST_ASSERT(cobalt_starts_with("hello", "hello") == 1);
    TEST_ASSERT(cobalt_starts_with("", "") == 1);
    TEST_ASSERT(cobalt_starts_with("hello", "") == 1);
    TEST_ASSERT(cobalt_starts_with("", "hello") == 0);
    TEST_ASSERT(cobalt_starts_with(NULL, "hello") == 0);
    TEST_ASSERT(cobalt_starts_with("hello", NULL) == 0);
    printf("  starts_with: OK\n");
}

void test_string_ends_with(void)
{
    printf("Testing cobalt_ends_with...\n");

    TEST_ASSERT(cobalt_ends_with("hello world", "world") == 1);
    TEST_ASSERT(cobalt_ends_with("hello world", "hello") == 0);
    TEST_ASSERT(cobalt_ends_with("hello", "hello") == 1);
    TEST_ASSERT(cobalt_ends_with("", "") == 1);
    TEST_ASSERT(cobalt_ends_with("hello", "") == 1);
    TEST_ASSERT(cobalt_ends_with("", "hello") == 0);
    TEST_ASSERT(cobalt_ends_with(NULL, "hello") == 0);
    TEST_ASSERT(cobalt_ends_with("hello", NULL) == 0);
    printf("  ends_with: OK\n");
}

void test_string_contains(void)
{
    printf("Testing cobalt_contains...\n");

    TEST_ASSERT(cobalt_contains("hello world", "world") == 1);
    TEST_ASSERT(cobalt_contains("hello world", "hello") == 1);
    TEST_ASSERT(cobalt_contains("hello world", "llo w") == 1);
    TEST_ASSERT(cobalt_contains("hello world", "xyz") == 0);
    TEST_ASSERT(cobalt_contains("", "") == 1);
    TEST_ASSERT(cobalt_contains("hello", "") == 1);
    TEST_ASSERT(cobalt_contains("", "hello") == 0);
    TEST_ASSERT(cobalt_contains(NULL, "hello") == 0);
    TEST_ASSERT(cobalt_contains("hello", NULL) == 0);
    printf("  contains: OK\n");
}

void test_string_snprintf(void)
{
    printf("Testing cobalt_snprintf...\n");

    char *buf = NULL;
    int   ret;

    ret = cobalt_snprintf(&buf, "hello %s", "world");
    TEST_ASSERT(ret == 11);
    TEST_ASSERT(buf != NULL);
    TEST_ASSERT(strcmp(buf, "hello world") == 0);
    free(buf);
    buf = NULL;

    ret = cobalt_snprintf(&buf, "int=%d float=%.1f", 42, 3.14);
    TEST_ASSERT(ret > 0);
    TEST_ASSERT(buf != NULL);
    TEST_ASSERT(strstr(buf, "42") != NULL);
    free(buf);
    buf = NULL;

    /* NULL inputs */
    TEST_ASSERT(cobalt_snprintf(NULL, "hello") == -1);
    TEST_ASSERT(cobalt_snprintf(&buf, NULL) == -1);

    printf("  snprintf: OK\n");
}

void test_string_vformat(void)
{
    printf("Testing cobalt_vformat...\n");

    /* vformat is a low-level va_list API; verify via snprintf delegation */
    char *buf = NULL;
    int   ret;

    ret = cobalt_snprintf(&buf, "%d + %d = %d", 1, 2, 3);
    TEST_ASSERT(ret == 9);
    TEST_ASSERT(buf != NULL);
    TEST_ASSERT(strcmp(buf, "1 + 2 = 3") == 0);
    free(buf);
    buf = NULL;

    printf("  vformat: OK\n");
}

void test_string_split(void)
{
    printf("Testing cobalt_split...\n");

    int    count = 0;
    char **parts = cobalt_split("a,b,c", ',', &count);
    TEST_ASSERT(parts != NULL);
    TEST_ASSERT(count == 3);
    TEST_ASSERT(strcmp(parts[0], "a") == 0);
    TEST_ASSERT(strcmp(parts[1], "b") == 0);
    TEST_ASSERT(strcmp(parts[2], "c") == 0);
    TEST_ASSERT(parts[3] == NULL);
    for (int i = 0; i < count; i++) {
        free(parts[i]);
    }
    free(parts);

    /* Empty string */
    parts = cobalt_split("", ',', &count);
    TEST_ASSERT(parts != NULL);
    TEST_ASSERT(count == 1);
    TEST_ASSERT(strcmp(parts[0], "") == 0);
    free(parts[0]);
    free(parts);

    /* Consecutive delimiters */
    parts = cobalt_split("a,,b", ',', &count);
    TEST_ASSERT(parts != NULL);
    TEST_ASSERT(count == 3);
    TEST_ASSERT(strcmp(parts[0], "a") == 0);
    TEST_ASSERT(strcmp(parts[1], "") == 0);
    TEST_ASSERT(strcmp(parts[2], "b") == 0);
    for (int i = 0; i < count; i++) {
        free(parts[i]);
    }
    free(parts);

    /* NULL input */
    TEST_ASSERT(cobalt_split(NULL, ',', &count) == NULL);

    printf("  split: OK\n");
}

void test_string_join(void)
{
    printf("Testing cobalt_join...\n");

    const char *parts[] = {"hello", "world", "foo", NULL};
    char       *result  = cobalt_join(parts, '-');
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(strcmp(result, "hello-world-foo") == 0);
    free(result);

    /* Single element */
    const char *single[] = {"only", NULL};
    result               = cobalt_join(single, '-');
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(strcmp(result, "only") == 0);
    free(result);

    /* NULL input */
    TEST_ASSERT(cobalt_join(NULL, '-') == NULL);

    printf("  join: OK\n");
}

void test_string_strip(void)
{
    printf("Testing cobalt_strip...\n");

    char *result = cobalt_strip("  hello world  ");
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(strcmp(result, "hello world") == 0);
    free(result);

    result = cobalt_strip("\t\n spaces \r\n");
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(strcmp(result, "spaces") == 0);
    free(result);

    result = cobalt_strip("no_whitespace");
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(strcmp(result, "no_whitespace") == 0);
    free(result);

    result = cobalt_strip("");
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(strcmp(result, "") == 0);
    free(result);

    TEST_ASSERT(cobalt_strip(NULL) == NULL);

    printf("  strip: OK\n");
}

/* -------------------------------------------------------------------------- */
/* Mock allocator for _with_alloc balance tests                               */
/* -------------------------------------------------------------------------- */

#define MOCK_STR_BUF_SIZE 4096
static char   mock_str_buf[MOCK_STR_BUF_SIZE];
static size_t mock_str_off         = 0;
static int    mock_str_alloc_count = 0;
static int    mock_str_free_count  = 0;

static void *mock_str_alloc(cobalt_allocator_t *self, size_t size)
{
    (void)self;
    mock_str_alloc_count++;
    if (mock_str_off + size > MOCK_STR_BUF_SIZE) {
        return NULL;
    }
    void *ptr = &mock_str_buf[mock_str_off];
    mock_str_off += size;
    return ptr;
}

static void mock_str_free(cobalt_allocator_t *self, void *ptr)
{
    (void)self;
    (void)ptr;
    mock_str_free_count++;
}

static void *mock_str_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
{
    (void)self;
    (void)ptr;
    (void)new_size;
    /* split realloc not exercised by current string funcs; return NULL = OOM */
    return NULL;
}

static cobalt_allocator_t mock_str_alloc_inst = {
    .alloc   = mock_str_alloc,
    .free    = mock_str_free,
    .realloc = mock_str_realloc,
};

static void mock_str_reset(void)
{
    mock_str_off         = 0;
    mock_str_alloc_count = 0;
    mock_str_free_count  = 0;
}

/* -------------------------------------------------------------------------- */
/* cobalt _with_alloc balance tests                                           */
/* -------------------------------------------------------------------------- */

void test_string_with_alloc(void)
{
    printf("Testing cobalt _with_alloc string APIs...\n");
    cobalt_allocator_t *a = &mock_str_alloc_inst;

    /* strdup_with_alloc */
    mock_str_reset();
    char *dup = cobalt_strdup_with_alloc("hello", a);
    TEST_ASSERT(dup != NULL);
    TEST_ASSERT(mock_str_alloc_count > 0);
    cobalt_allocator_free(a, dup);
    TEST_ASSERT(mock_str_alloc_count == mock_str_free_count);

    /* vformat_with_alloc / snprintf_with_alloc */
    mock_str_reset();
    char *buf = NULL;
    int   ret;
    ret = cobalt_snprintf_with_alloc(&buf, "hello %s", a, "world");
    TEST_ASSERT(ret == 11);
    TEST_ASSERT(buf != NULL);
    TEST_ASSERT(strcmp(buf, "hello world") == 0);
    cobalt_allocator_free(a, buf);
    TEST_ASSERT(mock_str_alloc_count == mock_str_free_count);

    /* split_with_alloc */
    mock_str_reset();
    int    count = 0;
    char **parts = cobalt_split_with_alloc("a,b,c", ',', &count, a);
    TEST_ASSERT(parts != NULL);
    TEST_ASSERT(count == 3);
    for (int i = 0; i < count; i++) {
        cobalt_allocator_free(a, parts[i]);
    }
    cobalt_allocator_free(a, parts);
    TEST_ASSERT(mock_str_alloc_count == mock_str_free_count);

    /* join_with_alloc */
    mock_str_reset();
    const char *p2[]   = {"x", "y", "z", NULL};
    char       *joined = cobalt_join_with_alloc(p2, '-', a);
    TEST_ASSERT(joined != NULL);
    TEST_ASSERT(strcmp(joined, "x-y-z") == 0);
    cobalt_allocator_free(a, joined);
    TEST_ASSERT(mock_str_alloc_count == mock_str_free_count);

    /* strip_with_alloc */
    mock_str_reset();
    char *stripped = cobalt_strip_with_alloc("  abc  ", a);
    TEST_ASSERT(stripped != NULL);
    TEST_ASSERT(strcmp(stripped, "abc") == 0);
    cobalt_allocator_free(a, stripped);
    TEST_ASSERT(mock_str_alloc_count == mock_str_free_count);

    /* NULL allocator falls back to system */
    char *fallback = cobalt_strdup_with_alloc("nopanic", NULL);
    TEST_ASSERT(fallback != NULL);
    free(fallback);

    printf("  _with_alloc balance: OK\n");
}

void test_string(void)
{
    printf("Testing string utilities...\n");
    test_string_duplicate();
    test_string_starts_with();
    test_string_ends_with();
    test_string_contains();
    test_string_snprintf();
    test_string_vformat();
    test_string_split();
    test_string_join();
    test_string_strip();
    test_string_with_alloc();
    printf("  String utility tests completed\n");
}
