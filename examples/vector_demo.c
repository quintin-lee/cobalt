/**
 * @file vector_demo.c
 * @brief Demonstrates using the Vector container
 *
 * Shows:
 * - Creating a vector
 * - Pushing elements
 * - Accessing by index
 * - Iterating through elements
 */

#include <cobalt/cobalt.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    /* Create a vector with initial capacity of 5 */
    cobalt_vector_t *vec = cobalt_vector_create(5);
    if (!vec) {
        fprintf(stderr, "Failed to create vector\n");
        return 1;
    }

    cobalt_info("Created vector with capacity %zu\n", vec->capacity);

    /* Add some integer values to the vector */
    int v1 = 10, v2 = 20, v3 = 30, v4 = 40, v5 = 50;
    cobalt_vector_push(vec, &v1);
    cobalt_vector_push(vec, &v2);
    cobalt_vector_push(vec, &v3);
    cobalt_vector_push(vec, &v4);
    cobalt_vector_push(vec, &v5);

    cobalt_info("Vector size: %zu\n", cobalt_vector_size(vec));

    /* Access elements by index */
    for (size_t i = 0; i < vec->size; i++) {
        int *val = (int *)cobalt_vector_get(vec, i);
        cobalt_info("vec[%zu] = %d\n", i, *val);
    }

    /* Update element at index 2 */
    int new_val = 999;
    cobalt_vector_set(vec, 2, &new_val);
    int *updated = (int *)cobalt_vector_get(vec, 2);
    cobalt_info("Updated vec[2] = %d\n", *updated);

    /* Destroy the vector */
    cobalt_vector_destroy(vec);

    cobalt_info("Vector demo complete!\n");
    return 0;
}