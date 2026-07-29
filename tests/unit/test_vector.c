/**
 * @file test_vector.c
 * @Unit test for vector container.
 */

#include <stdio.h>
#include "cobalt/container/vector.h"

void test_vector(void) {
    printf("Testing vector...\n");
    
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
    
    printf("  Vector tests completed\n");
}
