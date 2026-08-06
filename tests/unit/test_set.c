#include "cobalt/container/set.h"
#include "cobalt/container/hashmap.h"
#include "test_framework.h"
#include <stdio.h>

void test_set_basic(void)
{
    printf("Testing set basic operations...\n");

    cobalt_set_t *set = cobalt_set_create(16);
    TEST_ASSERT(set != NULL);
    TEST_ASSERT(cobalt_set_is_empty(set));
    TEST_ASSERT(cobalt_set_size(set) == 0);

    int a = 1, b = 2, c = 3;

    TEST_ASSERT(cobalt_set_insert(set, &a) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 1);
    TEST_ASSERT(cobalt_set_contains(set, &a));
    printf("  Insert and contains: OK\n");

    TEST_ASSERT(cobalt_set_insert(set, &b) == 0);
    TEST_ASSERT(cobalt_set_insert(set, &c) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 3);
    printf("  Multiple inserts: OK\n");

    TEST_ASSERT(cobalt_set_contains(set, &a));
    TEST_ASSERT(cobalt_set_contains(set, &b));
    TEST_ASSERT(cobalt_set_contains(set, &c));
    printf("  Contains check: OK\n");

    TEST_ASSERT(cobalt_set_remove(set, &b) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 2);
    TEST_ASSERT(!cobalt_set_contains(set, &b));
    printf("  Remove: OK\n");

    TEST_ASSERT(cobalt_set_remove(set, &b) == -1);
    printf("  Remove non-existent: OK\n");

    cobalt_set_destroy(set);
    printf("  Set basic test passed\n");
}

void test_set_duplicates(void)
{
    printf("Testing set duplicate handling...\n");

    cobalt_set_t *set = cobalt_set_create(4);
    TEST_ASSERT(set != NULL);

    int val = 42;
    TEST_ASSERT(cobalt_set_insert(set, &val) == 0);
    TEST_ASSERT(cobalt_set_insert(set, &val) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 1);

    cobalt_set_destroy(set);
    printf("  Duplicate insert (idempotent): OK\n");
}

void test_set_empty(void)
{
    printf("Testing set empty operations...\n");

    cobalt_set_t *set = cobalt_set_create(0);
    TEST_ASSERT(set != NULL);
    TEST_ASSERT(cobalt_set_is_empty(set));
    TEST_ASSERT(cobalt_set_size(set) == 0);

    TEST_ASSERT(cobalt_set_remove(set, NULL) == -1);
    TEST_ASSERT(cobalt_set_contains(set, NULL) == 0);

    cobalt_set_destroy(set);

    TEST_ASSERT(cobalt_set_contains(NULL, NULL) == 0);
    TEST_ASSERT(cobalt_set_size(NULL) == 0);
    cobalt_set_destroy(NULL);
    printf("  Empty and NULL safety: OK\n");
}

void test_set(void)
{
    printf("Testing set...\n");
    test_set_basic();
    test_set_duplicates();
    test_set_empty();
    printf("  Set tests completed\n");
}
