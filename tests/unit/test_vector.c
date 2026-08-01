/**
 * @file test_vector.c
 * @brief Unit test for vector container.
 */

#include <stdio.h>
#include "cobalt/container/vector.h"
#include "test_framework.h"

void test_vector_basic(void) {
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

    /* Clean up */
    cobalt_vector_destroy(vec);
    printf("  Vector destroyed\n");

    printf("  Vector basic tests completed\n");
}

void test_vector_edge_cases(void) {
    printf("Testing vector edge cases...\n");
    
    /* NULL operations */
    TEST_ASSERT(cobalt_vector_push(NULL, NULL) == -1);
    TEST_ASSERT(cobalt_vector_get(NULL, 0) == NULL);
    TEST_ASSERT(cobalt_vector_size(NULL) == 0);
    TEST_ASSERT(cobalt_vector_is_empty(NULL) == 0); /* NULL is not "empty", it's invalid */
    
    /* Index out of bounds */
    cobalt_vector_t *vec = cobalt_vector_create(2);
    TEST_ASSERT(cobalt_vector_get(vec, 99) == NULL);
    TEST_ASSERT(cobalt_vector_set(vec, 99, NULL) == -1);
    
    /* Multiple push/pop cycle */
    for (int i = 0; i < 100; i++) {
        int val = i;
        cobalt_vector_push(vec, &val);
    }
    TEST_ASSERT(cobalt_vector_size(vec) == 100);
    
    /* Get last element */
    int *last = (int *)cobalt_vector_get(vec, 99);
    TEST_ASSERT(last != NULL && *last == 99);
    
    cobalt_vector_destroy(vec);
    printf("  Vector edge cases OK\n");
}

void test_vector(void) {
    printf("Testing vector...\n");
    test_vector_basic();
    test_vector_edge_cases();
}
