#ifndef VECTOR_H
#define VECTOR_H

/**
 * @file vector.h
 * @brief Dynamic array (vector) container
 */

#include "cobalt/interface/sequence.h"

/* Vector is a sequence implementation */
typedef struct {
  cobalt_sequence_t base;       /* Base sequence interface */
  void **items;                 /* Array of items */
  size_t capacity;              /* Current capacity */
  size_t size;                  /* Number of elements */
} cobalt_vector_t;

/* Create a new vector */
cobalt_vector_t *cobalt_vector_create(size_t initial_capacity);

/* Destroy the vector */
void cobalt_vector_destroy(cobalt_vector_t *vec);

/* Add element to end */
int cobalt_vector_push(cobalt_vector_t *vec, void *item);

/* Get element by index */
void *cobalt_vector_get(cobalt_vector_t *vec, size_t index);

/* Set element by index */
int cobalt_vector_set(cobalt_vector_t *vec, size_t index, void *item);

/* Get vector size */
size_t cobalt_vector_size(cobalt_vector_t *vec);

/* Check if vector is empty */
int cobalt_vector_is_empty(cobalt_vector_t *vec);

#endif /* VECTOR_H */
