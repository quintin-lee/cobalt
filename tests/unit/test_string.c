/**
 * @file test_string.c
 * @brief Unit test for string utility functions.
 */

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
    char  *buf = NULL;
    int    ret;

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

    int count = 0;
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
    char *result = cobalt_join(parts, '-');
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(strcmp(result, "hello-world-foo") == 0);
    free(result);

    /* Single element */
    const char *single[] = {"only", NULL};
    result = cobalt_join(single, '-');
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
    printf("  String utility tests completed\n");
}

