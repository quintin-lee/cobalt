/**
 * @file test_iterator.c
 * @brief Unit test for iterator module.
 */

#include "cobalt/container/vector.h"
#include "cobalt/interface/iterator.h"
#include "cobalt/interface/sequence.h"
#include "cobalt/memory/allocator.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test sequence with manual implementation */
typedef struct {
    cobalt_sequence_t base;
    int              *data;
    size_t            count;
    size_t            capacity;
} test_seq_t;

static size_t test_seq_size(cobalt_sequence_t *self)
{
    test_seq_t *seq = (test_seq_t *)self;
    return seq->count;
}

static int test_seq_is_empty(cobalt_sequence_t *self)
{
    test_seq_t *seq = (test_seq_t *)self;
    return seq->count == 0;
}

static void test_seq_add(cobalt_sequence_t *self, void *item)
{
    test_seq_t *seq = (test_seq_t *)self;
    if (seq->count >= seq->capacity) {
        seq->capacity = seq->capacity ? seq->capacity * 2 : 4;
        int *new_data = realloc(seq->data, seq->capacity * sizeof(int));
        if (!new_data) {
            return;
        }
        seq->data = new_data;
    }
    seq->data[seq->count++] = *(int *)item;
}

static void test_seq_remove(cobalt_sequence_t *self, void *item)
{
    (void)item;
    test_seq_t *seq = (test_seq_t *)self;
    /* Simple implementation: just decrement size */
    if (seq->count > 0) {
        seq->count--;
    }
}

static void *test_seq_get_at_index(cobalt_sequence_t *self, size_t index)
{
    test_seq_t *seq = (test_seq_t *)self;
    if (index >= seq->count) {
        return NULL;
    }
    return &seq->data[index];
}

static cobalt_iterator_t *test_seq_iterator(cobalt_sequence_t *self)
{
    return cobalt_iterator_new(self);
}

static cobalt_sequence_t *test_seq_create(void)
{
    test_seq_t *seq = malloc(sizeof(test_seq_t));
    if (!seq) {
        return NULL;
    }
    seq->data     = NULL;
    seq->count    = 0;
    seq->capacity = 0;

    seq->base.size         = test_seq_size;
    seq->base.is_empty     = test_seq_is_empty;
    seq->base.add          = test_seq_add;
    seq->base.remove       = test_seq_remove;
    seq->base.iterator     = test_seq_iterator;
    seq->base.get_at_index = test_seq_get_at_index;

    return (cobalt_sequence_t *)seq;
}

static void test_seq_destroy(cobalt_sequence_t *seq)
{
    if (!seq) {
        return;
    }
    test_seq_t *tseq = (test_seq_t *)seq;
    free(tseq->data);
    free(tseq);
}

void test_iterator_basic(void)
{
    printf("Testing iterator basic operations...\n");

    /* Test with NULL sequence */
    cobalt_iterator_t *iter = cobalt_iterator_new(NULL);
    TEST_ASSERT(iter == NULL);
    printf("  Iterator with NULL seq returns NULL: OK\n");

    /* Create a sequence with data */
    cobalt_sequence_t *seq = test_seq_create();
    TEST_ASSERT(seq != NULL);
    printf("  Test sequence created\n");

    int values[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) {
        test_seq_add(seq, &values[i]);
    }
    TEST_EQUAL(test_seq_size(seq), (size_t)5);

    /* Create iterator */
    iter = cobalt_iterator_new(seq);
    TEST_ASSERT(iter != NULL);
    printf("  Iterator created: OK\n");

    /* Test has_next at start */
    TEST_ASSERT(cobalt_iterator_has_next(iter) != 0);
    printf("  has_next at start: OK\n");

    /* Iterate through all elements */
    int count = 0;
    while (cobalt_iterator_has_next(iter)) {
        void *item = cobalt_iterator_next(iter);
        TEST_ASSERT(item != NULL);
        TEST_EQUAL(*(int *)item, values[count]);
        count++;
    }

    TEST_EQUAL(count, 5);
    printf("  Iterated through 5 elements: OK\n");

    /* has_next should be false after exhausting */
    TEST_ASSERT(cobalt_iterator_has_next(iter) == 0);
    printf("  has_next after exhaustion: OK\n");

    /* next should return NULL when exhausted */
    void *null_item = cobalt_iterator_next(iter);
    TEST_ASSERT(null_item == NULL);
    printf("  next() returns NULL when exhausted: OK\n");

    cobalt_iterator_destroy(iter);
    test_seq_destroy(seq);
}

void test_iterator_null(void)
{
    printf("Testing iterator with NULL...\n");

    /* Test NULL iterator operations */
    TEST_ASSERT(cobalt_iterator_has_next(NULL) == 0);
    printf("  has_next(NULL) returns 0: OK\n");

    TEST_ASSERT(cobalt_iterator_next(NULL) == NULL);
    printf("  next(NULL) returns NULL: OK\n");

    /* Destroy NULL should not crash */
    cobalt_iterator_destroy(NULL);
    printf("  destroy(NULL): OK\n");
}

void test_iterator_with_vector(void)
{
    printf("Testing iterator with vector...\n");

    /* NOTE: the parameter is initial CAPACITY, not element size. */
    cobalt_vector_t *vec = cobalt_vector_create(5);
    TEST_ASSERT(vec != NULL);

    int values[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(cobalt_vector_push(vec, &values[i]) == 0);
    }
    TEST_EQUAL(cobalt_vector_size(vec), (size_t)5);

    /* Get iterator from vector (via sequence interface) */
    cobalt_sequence_t *seq  = (cobalt_sequence_t *)vec;
    cobalt_iterator_t *iter = cobalt_iterator_new(seq);
    TEST_ASSERT(iter != NULL);

    int count = 0;
    while (cobalt_iterator_has_next(iter)) {
        void *item = cobalt_iterator_next(iter);
        TEST_ASSERT(item != NULL);
        TEST_EQUAL(*(int *)item, values[count]);
        count++;
    }

    TEST_EQUAL(count, 5);
    printf("  Vector iterator traversed 5 elements: OK\n");

    cobalt_iterator_destroy(iter);
    cobalt_vector_destroy(vec);
}


static void test_iterator_with_allocator(void)
{
    printf("Testing iterator with custom allocator...\n");

    /* NULL sequence must still be rejected with a NULL allocator */
    TEST_ASSERT(cobalt_iterator_new_with_allocator(NULL, NULL) == NULL);

    cobalt_allocator_t *sys = cobalt_allocator_get_system();

    cobalt_sequence_t *seq = test_seq_create();
    TEST_ASSERT(seq != NULL);

    int values[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) {
        test_seq_add(seq, &values[i]);
    }

    /* NULL allocator falls back to the system allocator */
    cobalt_iterator_t *iter = cobalt_iterator_new_with_allocator(seq, NULL);
    TEST_ASSERT(iter != NULL);

    int count = 0;
    while (cobalt_iterator_has_next(iter)) {
        void *item = cobalt_iterator_next(iter);
        TEST_ASSERT(item != NULL);
        TEST_EQUAL(*(int *)item, values[count]);
        count++;
    }
    TEST_EQUAL(count, 5);
    printf("  NULL allocator traversed 5 elements: OK\n");

    /* destroy() releases through the allocator recorded at creation */
    cobalt_iterator_destroy(iter);
    printf("  Allocator-backed iterator destroyed: OK\n");

    /* Explicit system allocator round trip */
    iter = cobalt_iterator_new_with_allocator(seq, sys);
    TEST_ASSERT(iter != NULL);
    count = 0;
    while (cobalt_iterator_has_next(iter)) {
        (void)cobalt_iterator_next(iter);
        count++;
    }
    TEST_EQUAL(count, 5);
    cobalt_iterator_destroy(iter);
    printf("  System allocator round trip: OK\n");

    test_seq_destroy(seq);
}

void test_iterator(void)
{
    printf("Testing iterator...\n");
    test_iterator_basic();
    test_iterator_null();
    test_iterator_with_vector();
    test_iterator_with_allocator();
    printf("  Iterator tests completed\n");
}
