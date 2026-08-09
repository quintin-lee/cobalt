/**
 * @file test_vector.c
 * @brief Unit test for vector container.
 */

#include "cobalt/container/vector.h"
#include "cobalt/interface/iterator.h"
#include "test_framework.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_vector_basic(void)
{
    printf("Testing vector basic operations...\n");

    /* Create a new vector */
    cobalt_vector_t *vec = cobalt_vector_create(2);
    if (!vec) {
        fprintf(stderr, "ERROR: Failed to create vector\n");
        return;
    }
    printf("  Vector created successfully\n");

    /* Check it's empty */
    if (cobalt_vector_is_empty(vec)) {
        printf("  Vector is empty (correct)\n");
    } else {
        fprintf(stderr, "ERROR: Vector should be empty\n");
    }

    size_t sz = cobalt_vector_size(vec);
    if (sz == 0) {
        printf("  Size is 0 (correct)\n");
    } else {
        fprintf(stderr, "ERROR: Expected size 0, got %zu\n", sz);
    }

    /* Add an element */
    int val = 42;
    int ret = cobalt_vector_push(vec, &val);
    if (ret == 0) {
        printf("  Push succeeded\n");
    } else {
        fprintf(stderr, "ERROR: Push failed\n");
    }

    sz = cobalt_vector_size(vec);
    if (sz == 1) {
        printf("  Size is 1 after push (correct)\n");
    } else {
        fprintf(stderr, "ERROR: Expected size 1, got %zu\n", sz);
    }

    /* Retrieve element */
    int *got = (int *)cobalt_vector_get(vec, 0);
    if (got && *got == 42) {
        printf("  Retrieved value 42 correctly\n");
    } else {
        fprintf(stderr, "ERROR: Expected 42, got %p\n", got);
    }

    /* Set element */
    int new_val = 100;
    ret         = cobalt_vector_set(vec, 0, &new_val);
    if (ret == 0) {
        printf("  Set succeeded\n");
    } else {
        fprintf(stderr, "ERROR: Set failed\n");
    }

    got = (int *)cobalt_vector_get(vec, 0);
    if (got && *got == 100) {
        printf("  Retrieved updated value 100 correctly\n");
    }

    /* Clean up */
    cobalt_vector_destroy(vec);
    printf("  Vector destroyed\n");
}

void test_vector_edge_cases(void)
{
    printf("Testing vector edge cases...\n");

    /* NULL operations */
    int ret = cobalt_vector_push(NULL, NULL);
    if (ret == -1) {
        printf("  Push to NULL returns -1: OK\n");
    }

    ret = cobalt_vector_set(NULL, 0, NULL);
    if (ret == -1) {
        printf("  Set on NULL returns -1: OK\n");
    }

    void *item = cobalt_vector_get(NULL, 0);
    if (item == NULL) {
        printf("  Get from NULL returns NULL: OK\n");
    }

    size_t sz = cobalt_vector_size(NULL);
    if (sz == 0) {
        printf("  Size of NULL is 0: OK\n");
    }

    int empty = cobalt_vector_is_empty(NULL);
    printf("  Is empty of NULL: %d\n", empty);

    /* Index out of bounds */
    cobalt_vector_t *vec = cobalt_vector_create(2);
    item                 = cobalt_vector_get(vec, 99);
    if (item == NULL) {
        printf("  Get out of bounds returns NULL: OK\n");
    }

    ret = cobalt_vector_set(vec, 99, NULL);
    if (ret == -1) {
        printf("  Set out of bounds returns -1: OK\n");
    }

    cobalt_vector_destroy(vec);

    /* Test NULL destroy */
    cobalt_vector_destroy(NULL);
    printf("  Destroy NULL vector: OK\n");
}

void test_vector_growth(void)
{
    printf("Testing vector growth...\n");

    cobalt_vector_t *vec = cobalt_vector_create(1);
    if (!vec) {
        fprintf(stderr, "ERROR: Failed to create vector\n");
        return;
    }

    /* Push many elements to test dynamic growth */
    int *values = malloc(100 * sizeof(int));
    if (!values) {
        fprintf(stderr, "ERROR: Failed to allocate values\n");
        cobalt_vector_destroy(vec);
        return;
    }
    for (int i = 0; i < 100; i++) {
        values[i] = i;
        int ret   = cobalt_vector_push(vec, &values[i]);
        if (ret != 0) {
            fprintf(stderr, "ERROR: Push failed at index %d\n", i);
            break;
        }
    }

    size_t sz = cobalt_vector_size(vec);
    if (sz == 100) {
        printf("  Size is 100 after growth: OK\n");
    } else {
        fprintf(stderr, "ERROR: Expected size 100, got %zu\n", sz);
    }

    /* Verify all elements */
    int all_ok = 1;
    for (int i = 0; i < 100; i++) {
        int *item = (int *)cobalt_vector_get(vec, i);
        if (!item || *item != i) {
            all_ok = 0;
            break;
        }
    }

    if (all_ok) {
        printf("  All 100 elements correct: OK\n");
    } else {
        fprintf(stderr, "ERROR: Some elements incorrect\n");
    }

    cobalt_vector_destroy(vec);
    free(values);
}

void test_vector_iterator(void)
{
    printf("Testing vector iterator...\n");

    cobalt_vector_t *vec = cobalt_vector_create(4);
    if (!vec) {
        fprintf(stderr, "ERROR: Failed to create vector\n");
        return;
    }

    int values[] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        cobalt_vector_push(vec, &values[i]);
    }

    /* Get iterator through sequence interface */
    cobalt_sequence_t *seq  = (cobalt_sequence_t *)vec;
    cobalt_iterator_t *iter = cobalt_iterator_new(seq);

    if (iter) {
        int count = 0;
        while (cobalt_iterator_has_next(iter)) {
            cobalt_iterator_next(iter);
            count++;
        }
        if (count == 3) {
            printf("  Iterator traversed 3 elements: OK\n");
        }
        cobalt_iterator_destroy(iter);
    } else {
        printf("  Note: Iterator is NULL (check implementation)\n");
    }

    cobalt_vector_destroy(vec);
}

void test_vector_zero_capacity(void)
{
    printf("Testing vector zero capacity...\n");

    cobalt_vector_t *vec = cobalt_vector_create(0);
    TEST_ASSERT(vec != NULL);
    TEST_ASSERT(cobalt_vector_is_empty(vec));
    TEST_ASSERT(cobalt_vector_size(vec) == 0);

    int val = 42;
    TEST_ASSERT(cobalt_vector_push(vec, &val) == 0);
    TEST_ASSERT(cobalt_vector_size(vec) == 1);
    TEST_ASSERT(*(int *)cobalt_vector_get(vec, 0) == 42);

    cobalt_vector_destroy(vec);
    printf("  Vector zero capacity test passed\n");
}

void test_vector_remove(void)
{
    printf("Testing vector_remove...\n");

    cobalt_vector_t *vec = cobalt_vector_create(4);
    TEST_ASSERT(vec != NULL);

    int values[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(cobalt_vector_push(vec, &values[i]) == 0);
    }

    TEST_ASSERT(cobalt_vector_size(vec) == 5);

    /* Remove middle element using sequence interface */
    cobalt_sequence_t *seq = (cobalt_sequence_t *)vec;
    seq->remove(seq, &values[2]); /* Remove 30 */

    TEST_ASSERT(cobalt_vector_size(vec) == 4);
    TEST_ASSERT(*(int *)cobalt_vector_get(vec, 0) == 10);
    TEST_ASSERT(*(int *)cobalt_vector_get(vec, 1) == 20);
    TEST_ASSERT(*(int *)cobalt_vector_get(vec, 2) == 40);
    TEST_ASSERT(*(int *)cobalt_vector_get(vec, 3) == 50);

    /* Remove first element */
    seq->remove(seq, &values[0]);
    TEST_ASSERT(cobalt_vector_size(vec) == 3);
    TEST_ASSERT(*(int *)cobalt_vector_get(vec, 0) == 20);

    /* Remove last element */
    seq->remove(seq, &values[4]);
    TEST_ASSERT(cobalt_vector_size(vec) == 2);

    /* Remove non-existent element */
    int not_found = 999;
    seq->remove(seq, &not_found);
    TEST_ASSERT(cobalt_vector_size(vec) == 2); /* Size unchanged */

    cobalt_vector_destroy(vec);
    printf("  vector_remove test passed\n");
}

void test_vector_large_capacity(void)
{
    printf("Testing vector large capacity...\n");

    cobalt_vector_t *vec = cobalt_vector_create(10000);
    TEST_ASSERT(vec != NULL);

    int *values = malloc(10000 * sizeof(int));
    TEST_ASSERT(values != NULL);

    for (int i = 0; i < 10000; i++) {
        values[i] = i;
        TEST_ASSERT(cobalt_vector_push(vec, &values[i]) == 0);
    }
    TEST_ASSERT(cobalt_vector_size(vec) == 10000);

    for (int i = 0; i < 10000; i++) {
        int *got = (int *)cobalt_vector_get(vec, i);
        TEST_ASSERT(got != NULL);
        TEST_ASSERT(*got == i);
    }

    free(values);
    cobalt_vector_destroy(vec);
    printf("  Large capacity vector test passed\n");
}

void test_vector_alternating_push_set(void)
{
    printf("Testing vector alternating push/set...\n");

    cobalt_vector_t *vec = cobalt_vector_create(2);
    TEST_ASSERT(vec != NULL);

    int replacements[] = {100, 101, 102, 103, 104};

    int *orig[5];
    for (int i = 0; i < 5; i++) {
        int *v = malloc(sizeof(int));
        TEST_ASSERT(v != NULL);
        *v      = i + 1;
        orig[i] = v;
        TEST_ASSERT(cobalt_vector_push(vec, v) == 0);
    }
    TEST_ASSERT(cobalt_vector_size(vec) == 5);

    for (int i = 0; i < 5; i++) {
        int *item = (int *)cobalt_vector_get(vec, i);
        TEST_ASSERT(item != NULL);
        TEST_ASSERT(*item == i + 1);
        TEST_ASSERT(cobalt_vector_set(vec, i, &replacements[i]) == 0);
        TEST_ASSERT(*(int *)cobalt_vector_get(vec, i) == replacements[i]);
    }

    TEST_ASSERT(cobalt_vector_size(vec) == 5);
    for (int i = 0; i < 5; i++) {
        free(orig[i]);
    }
    cobalt_vector_destroy(vec);
    printf("  Alternating push/set test passed\n");
}


void test_vector_reserve(void)
{
    printf("Testing vector reserve and capacity...\n");

    cobalt_vector_t *vec = cobalt_vector_create(4);
    TEST_ASSERT(vec != NULL);

    TEST_ASSERT(cobalt_vector_capacity(vec) == 4);
    TEST_ASSERT(cobalt_vector_size(vec) == 0);

    /* Reserve more capacity */
    int ret = cobalt_vector_reserve(vec, 16);
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(cobalt_vector_capacity(vec) == 16);
    TEST_ASSERT(cobalt_vector_size(vec) == 0);

    /* Reserve less than current capacity — no-op */
    ret = cobalt_vector_reserve(vec, 8);
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(cobalt_vector_capacity(vec) == 16);

    /* Add elements */
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        ret = cobalt_vector_push(vec, &vals[i]);
        TEST_ASSERT(ret == 0);
    }
    TEST_ASSERT(cobalt_vector_size(vec) == 5);
    TEST_ASSERT(cobalt_vector_capacity(vec) == 16);

    /* Shrink to fit */
    ret = cobalt_vector_shrink_to_fit(vec);
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(cobalt_vector_capacity(vec) == 5);

    /* Shrink again — no-op */
    ret = cobalt_vector_shrink_to_fit(vec);
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(cobalt_vector_capacity(vec) == 5);

    /* Shrink empty vector */
    cobalt_vector_t *empty = cobalt_vector_create(10);
    TEST_ASSERT(empty != NULL);
    ret = cobalt_vector_shrink_to_fit(empty);
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(cobalt_vector_capacity(empty) == 0);
    cobalt_vector_destroy(empty);

    cobalt_vector_destroy(vec);
    printf("  Reserve/capacity/shrink: OK\n");
}

void test_vector(void)
{
    printf("Testing vector...\n");
    test_vector_basic();
    test_vector_edge_cases();
    test_vector_growth();
    test_vector_iterator();
    test_vector_zero_capacity();
    test_vector_remove();
    test_vector_large_capacity();
    test_vector_alternating_push_set();
}

