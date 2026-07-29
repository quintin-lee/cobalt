#include "cobalt/container/vector.h"
#include <stdlib.h>

/* Vector sequence operations */
static size_t vector_size_seq(cobalt_sequence_t *self) {
    cobalt_vector_t *vec = (cobalt_vector_t *)self;
    return vec->size;
}

static int vector_is_empty_seq(cobalt_sequence_t *self) {
    cobalt_vector_t *vec = (cobalt_vector_t *)self;
    return vec->size == 0;
}

static void vector_add_seq(cobalt_sequence_t *self, void *item) {
    cobalt_vector_t *vec = (cobalt_vector_t *)self;
    if (vec->size >= vec->capacity) {
        /* Double capacity (minimum 1) */
        size_t new_cap = (vec->capacity == 0) ? 1 : vec->capacity * 2;
        void **new_items = realloc(vec->items, new_cap * sizeof(void*));
        if (!new_items) return;
        vec->items = new_items;
        vec->capacity = new_cap;
    }
    vec->items[vec->size++] = item;
}

static void vector_remove_seq(cobalt_sequence_t *self, void *item) {
    /* Not implemented yet - stub */
    (void)self; (void)item;
}

static cobalt_iterator_t *vector_iterator_seq(cobalt_sequence_t *self) {
    /* Iterator not implemented yet - returns NULL placeholder */
    (void)self;
    return NULL;
}

/* Initialize the sequence base vtable for a vector instance */
static void vector_init_base(cobalt_vector_t *vec) {
    vec->base.size = vector_size_seq;
    vec->base.is_empty = vector_is_empty_seq;
    vec->base.add = vector_add_seq;
    vec->base.remove = vector_remove_seq;
    vec->base.iterator = vector_iterator_seq;
}

/* Create a new vector */
cobalt_vector_t *cobalt_vector_create(size_t initial_capacity) {
    cobalt_vector_t *vec = malloc(sizeof(cobalt_vector_t));
    if (!vec) return NULL;
    
    vec->items = malloc(initial_capacity * sizeof(void*));
    if (!vec->items) {
        free(vec);
        return NULL;
    }
    vec->capacity = initial_capacity;
    vec->size = 0;
    
    /* Initialize the sequence base */
    vector_init_base(vec);
    
    return vec;
}

/* Destroy the vector */
void cobalt_vector_destroy(cobalt_vector_t *vec) {
    if (vec) {
        free(vec->items);
        free(vec);
    }
}

/* Add element to end */
int cobalt_vector_push(cobalt_vector_t *vec, void *item) {
    if (!vec) return -1;
    vector_add_seq((cobalt_sequence_t *)vec, item);
    return 0;
}

/* Get element by index */
void *cobalt_vector_get(cobalt_vector_t *vec, size_t index) {
    if (!vec || index >= vec->size) return NULL;
    return vec->items[index];
}

/* Set element by index */
int cobalt_vector_set(cobalt_vector_t *vec, size_t index, void *item) {
    if (!vec || index >= vec->size) return -1;
    vec->items[index] = item;
    return 0;
}

/* Get vector size */
size_t cobalt_vector_size(cobalt_vector_t *vec) {
    if (!vec) return 0;
    return vec->size;
}

/* Check if vector is empty */
int cobalt_vector_is_empty(cobalt_vector_t *vec) {
    return vec && vec->size == 0;
}
