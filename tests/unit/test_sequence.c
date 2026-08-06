#include "cobalt/interface/iterator.h"
/**
 * @file test_sequence.c
 * @brief Unit test for sequence convenience API and deque sequence interface.
 */

#include "cobalt/container/deque.h"
#include "cobalt/interface/sequence.h"
#include "test_framework.h"
#include <stdio.h>

void test_sequence_convenience(void)
{
    printf("Testing sequence convenience API...\n");

    cobalt_deque_t *dq = cobalt_deque_create();
    TEST_ASSERT(dq != NULL);

    /* Deque implements cobalt_sequence_t */
    cobalt_sequence_t *seq = (cobalt_sequence_t *)dq;

    /* is_empty on empty deque */
    TEST_ASSERT(cobalt_sequence_is_empty(seq) == 1);
    TEST_ASSERT(cobalt_sequence_size(seq) == 0);
    printf("  Empty checks: OK\n");

    /* Add elements via sequence interface */
    int a = 10, b = 20, c = 30;
    TEST_ASSERT(cobalt_sequence_add(seq, &a) == 0);
    TEST_ASSERT(cobalt_sequence_add(seq, &b) == 0);
    TEST_ASSERT(cobalt_sequence_add(seq, &c) == 0);
    TEST_ASSERT(cobalt_sequence_size(seq) == 3);
    printf("  Add 3 elements: OK\n");

    TEST_ASSERT(cobalt_sequence_is_empty(seq) == 0);
    printf("  Non-empty check: OK\n");

    /* Remove by pointer equality */
    TEST_ASSERT(cobalt_sequence_remove(seq, &b) == 0);
    TEST_ASSERT(cobalt_sequence_size(seq) == 2);
    printf("  Remove middle: OK\n");

    /* Remove non-existent */
    int d = 99;
    TEST_ASSERT(cobalt_sequence_remove(seq, &d) == -1);
    TEST_ASSERT(cobalt_sequence_size(seq) == 2);
    printf("  Remove non-existent: OK\n");

    cobalt_deque_destroy(dq);
    printf("  Sequence convenience tests passed\n");
}

void test_deque_sequence_get_at_index(void)
{
    printf("Testing deque get_at_index via sequence interface...\n");

    cobalt_deque_t *dq = cobalt_deque_create();
    TEST_ASSERT(dq != NULL);

    cobalt_sequence_t *seq = (cobalt_sequence_t *)dq;

    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        cobalt_sequence_add(seq, &vals[i]);
    }

    /* Index 0 */
    void *item = seq->get_at_index(seq, 0);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 1);
    printf("  Index 0: OK\n");

    /* Index 2 (middle) */
    item = seq->get_at_index(seq, 2);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 3);
    printf("  Index 2: OK\n");

    /* Index 4 (last) */
    item = seq->get_at_index(seq, 4);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 5);
    printf("  Index 4: OK\n");

    /* Out of bounds */
    item = seq->get_at_index(seq, 10);
    TEST_ASSERT(item == NULL);
    printf("  Out of bounds: OK\n");

    cobalt_deque_destroy(dq);
    printf("  get_at_index tests passed\n");
}

void test_deque_sequence_iterator(void)
{
    printf("Testing deque iterator via sequence interface...\n");

    cobalt_deque_t *dq = cobalt_deque_create();
    TEST_ASSERT(dq != NULL);

    cobalt_sequence_t *seq    = (cobalt_sequence_t *)dq;
    int                vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        cobalt_sequence_add(seq, &vals[i]);
    }

    /* Use sequence->iterator to get a cobalt_iterator_t */
    cobalt_iterator_t *iter = seq->iterator(seq);
    TEST_ASSERT(iter != NULL);

    int count = 0;
    while (cobalt_iterator_has_next(iter)) {
        void *item = cobalt_iterator_next(iter);
        TEST_ASSERT(item != NULL);
        count++;
    }
    TEST_ASSERT(count == 3);
    printf("  Iterator traversed 3 elements: OK\n");

    cobalt_iterator_destroy(iter);
    cobalt_deque_destroy(dq);
    printf("  Iterator tests passed\n");
}

void test_sequence_null_safety(void)
{
    printf("Testing sequence null safety...\n");

    TEST_ASSERT(cobalt_sequence_size(NULL) == 0);
    TEST_ASSERT(cobalt_sequence_is_empty(NULL) == 1);
    TEST_ASSERT(cobalt_sequence_add(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_sequence_remove(NULL, NULL) == -1);
    printf("  Null safety: OK\n");
}

void test_sequence(void)
{
    printf("Testing sequence...\n");
    test_sequence_convenience();
    test_deque_sequence_get_at_index();
    test_deque_sequence_iterator();
    test_sequence_null_safety();
    printf("  Sequence tests completed\n");
}
