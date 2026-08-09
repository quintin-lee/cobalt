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

void test_list_remove_at(void);
void test_list_sequence_interface(void);
void test_list_null_safety(void);
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

void test_list_remove(void)
{
    printf("Testing list_remove...\n");

    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    int a = 1, b = 2, c = 3;

    TEST_ASSERT(cobalt_list_push_back(list, &a) == 0);
    TEST_ASSERT(cobalt_list_push_back(list, &b) == 0);
    TEST_ASSERT(cobalt_list_push_back(list, &c) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 3);

    TEST_ASSERT(cobalt_list_remove(list, &b) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 2);
    TEST_ASSERT(cobalt_list_get(list, 0) == &a);
    TEST_ASSERT(cobalt_list_get(list, 1) == &c);

    TEST_ASSERT(cobalt_list_remove(list, &a) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 1);
    TEST_ASSERT(cobalt_list_get(list, 0) == &c);

    TEST_ASSERT(cobalt_list_remove(list, &c) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 0);
    TEST_ASSERT(cobalt_list_is_empty(list));

    TEST_ASSERT(cobalt_list_remove(list, &a) == -1);

    int d = 4;
    TEST_ASSERT(cobalt_list_remove(list, &d) == -1);

    cobalt_list_destroy(list);
    printf("  List remove test passed\n");
}

static int predicate_gte2(const void *item, void *user_data)
{
    (void)user_data;
    return *(const int *)item >= 2;
}

void test_list_remove_if(void)
{
    printf("Testing list_remove_if...\n");

    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    int a = 1, b = 2, c = 3, d = 4;
    TEST_ASSERT(cobalt_list_push_back(list, &a) == 0);
    TEST_ASSERT(cobalt_list_push_back(list, &b) == 0);
    TEST_ASSERT(cobalt_list_push_back(list, &c) == 0);
    TEST_ASSERT(cobalt_list_push_back(list, &d) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 4);

    /* Remove first element >= 2 (should remove b=2) */
    TEST_ASSERT(cobalt_list_remove_if(list, predicate_gte2, NULL) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 3);
    TEST_ASSERT(cobalt_list_get(list, 0) == &a);
    TEST_ASSERT(cobalt_list_get(list, 1) == &c);
    TEST_ASSERT(cobalt_list_get(list, 2) == &d);

    /* Remove all elements >= 2 (removes c=3, then d=4) */
    while (cobalt_list_remove_if(list, predicate_gte2, NULL) == 0)
        ;
    TEST_ASSERT(cobalt_list_size(list) == 1);
    TEST_ASSERT(cobalt_list_get(list, 0) == &a);

    /* No more matches */
    TEST_ASSERT(cobalt_list_remove_if(list, predicate_gte2, NULL) == -1);

    /* NULL predicate should fail */
    TEST_ASSERT(cobalt_list_remove_if(list, NULL, NULL) == -1);
    TEST_ASSERT(cobalt_list_remove_if(NULL, predicate_gte2, NULL) == -1);

    cobalt_list_destroy(list);
    printf("  List remove_if test passed\n");
}

void test_list_sort(void);

void test_list(void)
{
    printf("Testing list...\n");
    test_list_basic();
    test_list_edge_cases();
    test_list_pop_back();
    test_list_get();
    test_list_iterator();
    test_list_remove();
    test_list_remove_if();
    test_list_sort();
    test_list_remove_at();
    test_list_sequence_interface();
    test_list_null_safety();
    printf("  List tests completed\n");
}

static int cmp_int_asc(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

void test_list_sort(void)
{
    printf("Testing list sort (merge sort)...\n");
    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    int vals[] = {5, 3, 8, 1, 9, 2, 7, 4, 6};
    for (int i = 0; i < 9; i++) {
        TEST_ASSERT(cobalt_list_push_back(list, &vals[i]) == 0);
    }
    TEST_ASSERT(cobalt_list_size(list) == 9);

    /* cobalt_list_sort works on the internal node head pointer */
    cobalt_list_node_t *node = cobalt_list_get_head(list);
    cobalt_list_sort((void **)&node, NULL, cmp_int_asc);
    cobalt_list_set_head(list, node, node); /* tail = node (will be updated by sort) */

    /* Verify sorted order by iterating */
    int                expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    cobalt_iterator_t *iter       = cobalt_list_iterator_create(list);
    TEST_ASSERT(iter != NULL);
    int idx = 0;
    while (cobalt_iterator_has_next(iter)) {
        int *got = (int *)cobalt_iterator_next(iter);
        TEST_ASSERT(got != NULL);
        TEST_EQUAL(*got, expected[idx]);
        idx++;
    }
    cobalt_iterator_destroy(iter);
    TEST_EQUAL(idx, 9);

    cobalt_list_destroy(list);
    printf("  list sort: OK\n");
}


void test_list_remove_at(void)
{
    printf("Testing list remove_at...\n");
    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    int a = 1, b = 2, c = 3, d = 4;
    cobalt_list_push_back(list, &a);
    cobalt_list_push_back(list, &b);
    cobalt_list_push_back(list, &c);
    cobalt_list_push_back(list, &d);
    TEST_ASSERT(cobalt_list_size(list) == 4);

    /* Remove middle element (index 1) */
    TEST_ASSERT(cobalt_list_remove_at(list, 1) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 3);
    TEST_ASSERT(*(int *)cobalt_list_get(list, 1) == 3);
    TEST_ASSERT(*(int *)cobalt_list_get(list, 2) == 4);

    /* Remove first element */
    TEST_ASSERT(cobalt_list_remove_at(list, 0) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 2);
    TEST_ASSERT(*(int *)cobalt_list_get(list, 0) == 3);

    /* Remove last element */
    TEST_ASSERT(cobalt_list_remove_at(list, 1) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 1);
    TEST_ASSERT(*(int *)cobalt_list_get(list, 0) == 3);

    /* Remove last remaining */
    TEST_ASSERT(cobalt_list_remove_at(list, 0) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 0);

    /* Out of bounds */
    TEST_ASSERT(cobalt_list_remove_at(list, 0) == -1);
    TEST_ASSERT(cobalt_list_remove_at(list, 99) == -1);

    cobalt_list_destroy(list);
    printf("  List remove_at test passed\n");
}

void test_list_sequence_interface(void)
{
    printf("Testing list sequence interface...\n");

    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    cobalt_sequence_t *seq = (cobalt_sequence_t *)list;

    TEST_ASSERT(seq->size(seq) == 0);
    TEST_ASSERT(seq->is_empty(seq) == 1);

    int a = 1, b = 2;
    seq->add(seq, &a);
    seq->add(seq, &b);
    TEST_ASSERT(seq->size(seq) == 2);
    TEST_ASSERT(seq->is_empty(seq) == 0);

    void *item = seq->get_at_index(seq, 0);
    TEST_ASSERT(item != NULL && *(int *)item == 1);

    item = seq->get_at_index(seq, 1);
    TEST_ASSERT(item != NULL && *(int *)item == 2);

    cobalt_iterator_t *iter = seq->iterator(seq);
    TEST_ASSERT(iter != NULL);
    int count = 0;
    while (cobalt_iterator_has_next(iter)) {
        cobalt_iterator_next(iter);
        count++;
    }
    TEST_ASSERT(count == 2);
    cobalt_iterator_destroy(iter);

    seq->remove(seq, &a);
    TEST_ASSERT(seq->size(seq) == 1);

    cobalt_list_destroy(list);
    printf("  List sequence interface: OK\n");
}

void test_list_null_safety(void)
{
    printf("Testing list NULL safety...\n");

    TEST_ASSERT(cobalt_list_push_front(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_list_push_back(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_list_pop_front(NULL) == NULL);
    TEST_ASSERT(cobalt_list_pop_back(NULL) == NULL);
    TEST_ASSERT(cobalt_list_get(NULL, 0) == NULL);
    TEST_ASSERT(cobalt_list_size(NULL) == 0);
    TEST_ASSERT(cobalt_list_is_empty(NULL) == 0);
    TEST_ASSERT(cobalt_list_remove(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_list_remove_if(NULL, NULL, NULL) == -1);
    TEST_ASSERT(cobalt_list_remove_at(NULL, 0) == -1);
    cobalt_list_destroy(NULL);

    printf("  List NULL safety: OK\n");
}
