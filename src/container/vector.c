#include "container/vector.h"
#include <stdlib.h>

static size_t vector_size(cobalt_sequence_t *self) {
  cobalt_vector_t *vec = (cobalt_vector_t *)self;
  return vec->size;
}

static int vector_is_empty(cobalt_sequence_t *self) {
  cobalt_vector_t *vec = (cobalt_vector_t *)self;
  return vec->size == 0;
}

static void vector_add(cobalt_sequence_t *self, void *item) {
  cobalt_vector_t *vec = (cobalt_vector_t *)self;
  if (vec->size >= vec->capacity) {
    vec->capacity = vec->capacity * 2;
    vec->items = realloc(vec->items, vec->capacity * sizeof(void*));
  }
  vec->items[vec->size++] = item;
}

static void vector_remove(cobalt_sequence_t *self, void *item) {
  (void)self; (void)item;
}

static cobalt_iterator_t *vector_iterator(cobalt_sequence_t *self) {
  (void)self;
  return NULL;
}

static const cobalt_sequence_ops vector_ops = {
  .size = vector_size,
  .is_empty = vector_is_empty,
  .add = vector_add,
  .remove = vector_remove,
  .iterator = vector_iterator
};

cobalt_vector_t *cobalt_vector_create(size_t initial_capacity) {
  cobalt_vector_t *vec = malloc(sizeof(cobalt_vector_t));
  if (!vec) return NULL;
  
  vec->base.ops = &vector_ops;
  vec->items = malloc(initial_capacity * sizeof(void*));
  if (!vec->items) {
    free(vec);
    return NULL;
  }
  vec->capacity = initial_capacity;
  vec->size = 0;
  return vec;
}

void cobalt_vector_destroy(cobalt_vector_t *vec) {
  if (vec) {
    free(vec->items);
    free(vec);
  }
}

int cobalt_vector_push(cobalt_vector_t *vec, void *item) {
  if (!vec) return -1;
  vector_add((cobalt_sequence_t *)vec, item);
  return 0;
}

void *cobalt_vector_get(cobalt_vector_t *vec, size_t index) {
  if (!vec || index >= vec->size) return NULL;
  return vec->items[index];
}

int cobalt_vector_set(cobalt_vector_t *vec, size_t index, void *item) {
  if (!vec || index >= vec->size) return -1;
  vec->items[index] = item;
  return 0;
}

size_t cobalt_vector_size(cobalt_vector_t *vec) {
  return vec ? vec->size : 0;
}

int cobalt_vector_is_empty(cobalt_vector_t *vec) {
  return vec && vec->size == 0;
}
