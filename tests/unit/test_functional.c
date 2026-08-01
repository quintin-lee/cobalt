/**
 * @file test_functional.c
 * @Unit test for functional utilities and predicates.
 */

#include "cobalt/algorithm/functional.h"
#include <stdio.h>

/* Sample comparator */
int simple_cmp(const void* a, const void* b)
{
    return (*(int*)a > *(int*)b) - (*(int*)a < *(int*)b);
}

void test_predicates(void)
{
    printf("Testing predicate functions...\n");

    int a = 5, b = 5;
    if (predicate_equal(&a, &b, (compare_func_t)simple_cmp))
        {
            printf("  Equal values: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: Equal values should match\n");
        }

    int c = 10;
    if (!predicate_equal(&a, &c, (compare_func_t)simple_cmp))
        {
            printf("  Different values not equal: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: Different values should not be equal\n");
        }

    if (predicate_not_equal(&a, &c, (compare_func_t)simple_cmp))
        {
            printf("  Not equal works: OK\n");
        }

    /* NULL predicates */
    if (predicate_null(NULL))
        {
            printf("  NULL check true: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: NULL should return true\n");
        }

    if (predicate_nonnull(&a))
        {
            printf("  Non-null check true: OK\n");
        }
}

void test_functional(void)
{
    printf("Testing functional...\n");
    test_predicates();
    printf("  Functional tests completed\n");
}
