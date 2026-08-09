/**
 * @file test_stream.c
 * @brief Unit test for stream processing utilities.
 */

#include "cobalt/algorithm/stream.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

static int pred_less_than_4(const void *item)
{
    return *(const int *)item < 4;
}

static int pred_less_than_3(const void *item)
{
    return *(const int *)item < 3;
}

static int pred_greater_than_0(const void *item)
{
    return *(const int *)item > 0;
}

static int pred_greater_than_10(const void *item)
{
    return *(const int *)item > 10;
}

static int pred_always_true(const void *item)
{
    (void)item;
    return 1;
}

static int pred_always_false(const void *item)
{
    (void)item;
    return 0;
}

void test_stream_take(void)
{
    printf("Testing cobalt_stream_take...\n");

    int input[] = {1, 2, 3, 4, 5};
    int output[5];

    cobalt_stream_take(input, output, 3, 5, sizeof(int));
    TEST_ASSERT(output[0] == 1);
    TEST_ASSERT(output[1] == 2);
    TEST_ASSERT(output[2] == 3);
    printf("  Take 3 from 5: OK\n");

    /* Take all */
    cobalt_stream_take(input, output, 5, 5, sizeof(int));
    TEST_ASSERT(output[4] == 5);
    printf("  Take all: OK\n");

    /* Take more than available */
    cobalt_stream_take(input, output, 10, 5, sizeof(int));
    TEST_ASSERT(output[4] == 5);
    printf("  Take more than available: OK\n");

    /* Null safety */
    cobalt_stream_take(NULL, output, 3, 5, sizeof(int));
    cobalt_stream_take(input, NULL, 3, 5, sizeof(int));
    printf("  Null safety: OK\n");
}

void test_stream_drop(void)
{
    printf("Testing cobalt_stream_drop...\n");

    int    input[] = {1, 2, 3, 4, 5};
    int    output[5];
    size_t count;

    cobalt_stream_drop(input, output, 2, 5, sizeof(int), &count);
    TEST_ASSERT(count == 3);
    TEST_ASSERT(output[0] == 3);
    TEST_ASSERT(output[1] == 4);
    TEST_ASSERT(output[2] == 5);
    printf("  Drop 2 from 5: OK\n");

    /* Drop all */
    cobalt_stream_drop(input, output, 5, 5, sizeof(int), &count);
    TEST_ASSERT(count == 0);
    printf("  Drop all: OK\n");

    /* Drop more than available */
    cobalt_stream_drop(input, output, 10, 5, sizeof(int), &count);
    TEST_ASSERT(count == 0);
    printf("  Drop more than available: OK\n");

    /* Null output */
    cobalt_stream_drop(input, NULL, 2, 5, sizeof(int), NULL);
    printf("  Null safety: OK\n");
}

void test_stream_take_while(void)
{
    printf("Testing cobalt_stream_take_while...\n");

    int    input[] = {1, 2, 3, 4, 5};
    int    output[5];
    size_t count;

    /* Take while < 4: should get {1, 2, 3} */
    count = 5;
    cobalt_stream_take_while(input, output, &count, sizeof(int), pred_less_than_4);
    TEST_ASSERT(count == 3);
    TEST_ASSERT(output[0] == 1);
    TEST_ASSERT(output[1] == 2);
    TEST_ASSERT(output[2] == 3);
    printf("  Take while < 4: OK\n");

    /* All match */
    count = 5;
    cobalt_stream_take_while(input, output, &count, sizeof(int), pred_always_true);
    TEST_ASSERT(count == 5);
    printf("  All match: OK\n");

    /* None match */
    count = 5;
    cobalt_stream_take_while(input, output, &count, sizeof(int), pred_always_false);
    TEST_ASSERT(count == 0);
    printf("  None match: OK\n");

    /* Null safety */
    cobalt_stream_take_while(NULL, output, &count, sizeof(int), pred_always_true);
    cobalt_stream_take_while(input, NULL, &count, sizeof(int), pred_always_true);
    printf("  Null safety: OK\n");
}

void test_stream_drop_while(void)
{
    printf("Testing cobalt_stream_drop_while...\n");

    int    input[] = {1, 2, 3, 4, 5};
    int    output[5];
    size_t count;

    /* Drop while < 3: should get {3, 4, 5} */
    count = 5;
    cobalt_stream_drop_while(input, output, &count, sizeof(int), pred_less_than_3);
    TEST_ASSERT(count == 3);
    TEST_ASSERT(output[0] == 3);
    TEST_ASSERT(output[1] == 4);
    TEST_ASSERT(output[2] == 5);
    printf("  Drop while < 3: OK\n");

    /* All match (drop all) */
    count = 5;
    cobalt_stream_drop_while(input, output, &count, sizeof(int), pred_always_true);
    TEST_ASSERT(count == 0);
    printf("  All match (drop all): OK\n");

    /* None match (keep all) */
    count = 5;
    cobalt_stream_drop_while(input, output, &count, sizeof(int), pred_always_false);
    TEST_ASSERT(count == 5);
    TEST_ASSERT(output[0] == 1);
    printf("  None match (keep all): OK\n");

    /* Null safety */
    cobalt_stream_drop_while(NULL, output, &count, sizeof(int), pred_always_true);
    cobalt_stream_drop_while(input, NULL, &count, sizeof(int), pred_always_true);
    printf("  Null safety: OK\n");
}

void test_stream(void)
{
    printf("Testing stream module...\n");
    test_stream_take();
    test_stream_drop();
    test_stream_take_while();
    test_stream_drop_while();
    printf("  Stream tests completed\n");
}

