/**
 * @file test_list.c
 * @brief Unit test for doubly-linked list container.
 */

#include "cobalt/container/list.h"
#include "cobalt/interface/iterator.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_list_basic(void)
{
    printf("Testing list basic operations...\n");

    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    /* Should be empty initially */
    TEST_ASSERT(cobalt_list_is_empty(list) == 1);
    TEST_ASSERT(cobalt_list_size(list) == 0);
    printf("  Initial state: OK\n");

    /* Push front */
    int val1 = 10;
    int ret  = cobalt_list_push_front(list, &val1);
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(cobalt_list_size(list) == 1);
    printf("  Push front: OK\n");

    /* Push back */
    int val2 = 20;
    ret      = cobalt_list_push_back(list, &val2);
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(cobalt_list_size(list) == 2);
    printf("  Push back: OK\n");

    /* Pop front returns 10 */
    void *item = cobalt_list_pop_front(list);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 10);
    TEST_ASSERT(cobalt_list_size(list) == 1);
    printf("  Pop front returns 10: OK\n");

    /* Pop back returns 20 */
    item = cobalt_list_pop_back(list);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 20);
    TEST_ASSERT(cobalt_list_size(list) == 0);
    printf("  Pop back returns 20: OK\n");

    /* Should be empty again */
    TEST_ASSERT(cobalt_list_is_empty(list) == 1);

    cobalt_list_destroy(list);
}

void test_list_edge_cases(void)
{
    printf("Testing list edge cases...\n");

    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    /* Pop from empty list should return NULL */
    void *item = cobalt_list_pop_front(list);
    TEST_ASSERT(item == NULL);
    printf("  Pop empty list returns NULL: OK\n");

    item = cobalt_list_pop_back(list);
    TEST_ASSERT(item == NULL);
    printf("  Pop back empty list returns NULL: OK\n");

    /* Get from empty list */
    item = cobalt_list_get(list, 0);
    TEST_ASSERT(item == NULL);
    printf("  Get from empty list returns NULL: OK\n");

    /* Push to NULL list should fail safely */
    int val = 42;
    int ret = cobalt_list_push_front(NULL, &val);
    TEST_ASSERT(ret == -1);
    printf("  Push to NULL list fails gracefully: OK\n");

    cobalt_list_destroy(list);

    /* Test NULL destroy */
    cobalt_list_destroy(NULL);
    printf("  Destroy NULL list: OK\n");
}

void test_list_pop_back(void)
{
    printf("Testing list_pop_back...\n");

    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    int val1 = 10;
    int val2 = 20;
    int val3 = 30;

    TEST_ASSERT(cobalt_list_push_back(list, &val1) == 0);
    TEST_ASSERT(cobalt_list_push_back(list, &val2) == 0);
    TEST_ASSERT(cobalt_list_push_back(list, &val3) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 3);

    /* Pop back should return last element (30) */
    void *item = cobalt_list_pop_back(list);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 30);
    TEST_ASSERT(cobalt_list_size(list) == 2);

    /* Pop back again should return 20 */
    item = cobalt_list_pop_back(list);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 20);
    TEST_ASSERT(cobalt_list_size(list) == 1);

    /* Pop back again should return 10 */
    item = cobalt_list_pop_back(list);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 10);
    TEST_ASSERT(cobalt_list_size(list) == 0);

    /* Pop back from empty list should return NULL */
    item = cobalt_list_pop_back(list);
    TEST_ASSERT(item == NULL);

    cobalt_list_destroy(list);
    printf("  list_pop_back test passed\n");
}

void test_list_get(void)
{
    printf("Testing list_get...\n");

    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    int values[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(cobalt_list_push_back(list, &values[i]) == 0);
    }

    /* Test getting elements from beginning */
    void *item = cobalt_list_get(list, 0);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 10);
    printf("  Get index 0 returns 10: OK\n");

    /* Test getting element from middle */
    item = cobalt_list_get(list, 2);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 30);
    printf("  Get index 2 returns 30: OK\n");

    /* Test getting element from end */
    item = cobalt_list_get(list, 4);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 50);
    printf("  Get index 4 returns 50: OK\n");

    /* Test out of bounds */
    item = cobalt_list_get(list, 5);
    TEST_ASSERT(item == NULL);
    printf("  Get out of bounds returns NULL: OK\n");

    item = cobalt_list_get(list, 100);
    TEST_ASSERT(item == NULL);
    printf("  Get far out of bounds returns NULL: OK\n");

    /* Test empty list */
    cobalt_list_t *empty = cobalt_list_create();
    TEST_ASSERT(empty != NULL);
    item = cobalt_list_get(empty, 0);
    TEST_ASSERT(item == NULL);
    printf("  Get from empty list returns NULL: OK\n");
    cobalt_list_destroy(empty);

    cobalt_list_destroy(list);
    printf("  list_get test passed\n");
}

void test_list_iterator(void)
{
    printf("Testing list_iterator...\n");

    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    int values[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(cobalt_list_push_back(list, &values[i]) == 0);
    }

    /* Get list-specific iterator */
    cobalt_iterator_t *iter = cobalt_list_iterator_create(list);
    TEST_ASSERT(iter != NULL);

    int count = 0;
    while (cobalt_iterator_has_next(iter)) {
        void *item = cobalt_iterator_next(iter);
        TEST_ASSERT(item != NULL);
        count++;
    }

    TEST_ASSERT(count == 5);
    printf("  Iterator traversed 5 elements: OK\n");

    /* Test has_next after exhaustion */
    TEST_ASSERT(cobalt_iterator_has_next(iter) == 0);
    TEST_ASSERT(cobalt_iterator_next(iter) == NULL);

    cobalt_iterator_destroy(iter);
    cobalt_list_destroy(list);
    printf("  list_iterator test passed\n");
}

void test_list(void)
{
    printf("Testing list...\n");
    test_list_basic();
    test_list_edge_cases();
    test_list_pop_back();
    test_list_get();
    test_list_iterator();
    printf("  List tests completed\n");
}
