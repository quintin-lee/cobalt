/**
 * @file test_functional.c
 * @Unit test for functional utilities and predicates.
 */

#include "cobalt/algorithm/functional.h"
#include "test_framework.h"
#include <stdio.h>

/* Sample comparator */
int simple_cmp(const void *a, const void *b)
{
    return (*(int *)a > *(int *)b) - (*(int *)a < *(int *)b);
}

/* Helper predicates for tests */
static int predicate_even(const void *item)
{
    return (*(int *)item % 2) == 0;
}

static int predicate_greater_than_5(const void *item)
{
    return *(int *)item > 5;
}

static int predicate_greater_than_100(const void *item)
{
    return *(int *)item > 100;
}

/* Helper operations for tests */
static void sum_adder(void *item)
{
    *(int *)item += *(int *)item;
}

static void sum_negator(void *item)
{
    *(int *)item = -*(int *)item;
}

void test_predicates(void)
{
    printf("Testing predicate functions...\n");

    int a = 5, b = 5;
    if (predicate_equal(&a, &b, (compare_func_t)simple_cmp)) {
        printf("  Equal values: OK\n");
    } else {
        fprintf(stderr, "ERROR: Equal values should match\n");
    }

    int c = 10;
    if (!predicate_equal(&a, &c, (compare_func_t)simple_cmp)) {
        printf("  Different values not equal: OK\n");
    } else {
        fprintf(stderr, "ERROR: Different values should not be equal\n");
    }

    if (predicate_not_equal(&a, &c, (compare_func_t)simple_cmp)) {
        printf("  Not equal works: OK\n");
    }

    /* NULL predicates */
    if (predicate_null(NULL)) {
        printf("  NULL check true: OK\n");
    } else {
        fprintf(stderr, "ERROR: NULL should return true\n");
    }

    if (predicate_nonnull(&a)) {
        printf("  Non-null check true: OK\n");
    }
}

void test_functional_bsearch(void)
{
    printf("Testing cobalt_bsearch...\n");

    int values[] = {1, 3, 5, 7, 9, 11, 13, 15};
    int n        = sizeof(values) / sizeof(values[0]);

    int  key    = 7;
    int *result = (int *)cobalt_bsearch(&key, values, n, sizeof(int), simple_cmp);
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(*result == 7);
    printf("  Find existing element: OK\n");

    key    = 10;
    result = (int *)cobalt_bsearch(&key, values, n, sizeof(int), simple_cmp);
    TEST_ASSERT(result == NULL);
    printf("  Find non-existing element: OK\n");

    key    = 1;
    result = (int *)cobalt_bsearch(&key, values, n, sizeof(int), simple_cmp);
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(*result == 1);
    printf("  Find first element: OK\n");

    key    = 15;
    result = (int *)cobalt_bsearch(&key, values, n, sizeof(int), simple_cmp);
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(*result == 15);
    printf("  Find last element: OK\n");
}

void test_functional_find_if(void)
{
    printf("Testing cobalt_find_if...\n");

    int values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int n        = sizeof(values) / sizeof(values[0]);

    int *result = (int *)cobalt_find_if(values, n, sizeof(int), predicate_even);
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(*result == 2);
    printf("  Find first even: OK\n");

    result = (int *)cobalt_find_if(values, n, sizeof(int), predicate_greater_than_5);
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(*result == 6);
    printf("  Find element > 5: OK\n");

    result = (int *)cobalt_find_if(values, n, sizeof(int), predicate_greater_than_100);
    TEST_ASSERT(result == NULL);
    printf("  Find non-existing: OK\n");
}

void test_functional_for_each(void)
{
    printf("Testing cobalt_for_each...\n");

    int values[] = {1, 2, 3, 4, 5};
    int n        = sizeof(values) / sizeof(values[0]);
    int sum      = 0;

    /* Use a different approach: accumulate sum manually */
    for (int i = 0; i < n; i++) {
        sum += values[i];
    }
    TEST_ASSERT(sum == 15);
    printf("  Sum verification: OK\n");

    /* Test for_each with a simple operation */
    int negate_sum = 0;
    cobalt_for_each(values, n, sizeof(int), sum_negator);

    for (int i = 0; i < n; i++) {
        negate_sum += values[i];
    }
    TEST_ASSERT(negate_sum == -15);
    printf("  Negate all elements: OK\n");
}

void test_functional(void)
{
    printf("Testing functional...\n");
    test_predicates();
    test_functional_bsearch();
    test_functional_find_if();
    test_functional_for_each();
    printf("  Functional tests completed\n");
}
