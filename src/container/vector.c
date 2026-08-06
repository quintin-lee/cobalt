/**
 * @file vector.c
 * @brief Implementation file of the dynamic array (Vector) container
 *
 * Implements the dynamic array interfaces defined in vector.h, including basic operation functions
 * as well as the implementation of its base class interface cobalt_sequence_t.
 */

#include "cobalt/container/vector.h"
#include "cobalt/interface/iterator.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Internal dynamic array structure, used to hide specific implementation details
 */
typedef struct {
    cobalt_sequence_t base; /**< Base sequence interface, must be placed at the beginning of the
                               structure to support polymorphic conversion */
    void **items;
    size_t capacity;
    size_t size;
} cobalt_vector_impl_t;

/**
 * @brief Public opaque type layout — matches cobalt_vector_impl_t exactly.
 * @details This definition lives in the .c file so users cannot access
 *          vector internals. The cast from cobalt_vector_t* to
 *          cobalt_vector_impl_t* is safe because both share the same
 *          starting cobalt_sequence_t base.
 */
struct cobalt_vector {
    cobalt_sequence_t base;
    void            **items;
    size_t            capacity;
    size_t            size;
};

/* ========================================================================= */
/* Sequence Interface specific implementation                                  */
/* ========================================================================= */

/**
 * @brief Get the sequence size (Sequence interface implementation)
 * @param self Sequence base class pointer
 * @return Number of elements in the dynamic array
 */
static size_t vector_size_seq(cobalt_sequence_t *self)
{
    cobalt_vector_impl_t *vec = (cobalt_vector_impl_t *)self;
    return vec->size;
}

/**
 * @brief Check if the sequence is empty (Sequence interface implementation)
 * @param self Sequence base class pointer
 * @return Returns 1 if empty, 0 otherwise
 */
static int vector_is_empty_seq(cobalt_sequence_t *self)
{
    cobalt_vector_impl_t *vec = (cobalt_vector_impl_t *)self;
    return vec->size == 0;
}

/**
 * @brief Add an element to the sequence (Sequence interface implementation)
 * @param self Sequence base class pointer
 * @param item Pointer to the element to be added
 *
 * When the capacity is insufficient, it will automatically expand using a 2x strategy.
 * If memory allocation fails, the COBALT_ERROR_OUT_OF_MEMORY error will be set.
 */
static void vector_add_seq(cobalt_sequence_t *self, void *item)
{
    cobalt_vector_impl_t *vec = (cobalt_vector_impl_t *)self;
    // Check if expansion is needed
    if (vec->size >= vec->capacity) {
        // Capacity doubling strategy; if current capacity is 0, initially allocate 1
        size_t new_cap   = (vec->capacity == 0) ? 1 : vec->capacity * 2;
        void **new_items = (void **)realloc(vec->items, new_cap * sizeof(void *));
        if (!new_items) {
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return;
        }
        vec->items    = new_items;
        vec->capacity = new_cap;
    }
    // Add element and update size
    vec->items[vec->size++] = item;
}

/**
 * @brief Remove the specified element from the sequence (Sequence interface implementation)
 * @param self Sequence base class pointer
 * @param item Pointer to the element to be removed
 *
 * Linearly searches for the element to remove. If found, shifts all elements after it forward by
 * one position. Only removes the first matching element.
 */
static void vector_remove_seq(cobalt_sequence_t *self, void *item)
{
    cobalt_vector_impl_t *vec = (cobalt_vector_impl_t *)self;
    if (!vec || !item) {
        return;
    }

    // Iterate to find the target element
    for (size_t i = 0; i < vec->size; i++) {
        if (vec->items[i] == item) {
            // Move all elements after the target element forward to overwrite
            memmove(vec->items + i, vec->items + i + 1, (vec->size - i - 1) * sizeof(void *));
            vec->size--;
            return; // Only remove the first match
        }
    }
}

/**
 * @brief Get the iterator of the sequence (Sequence interface implementation)
 * @param self Sequence base class pointer
 * @return Returns a pointer to the newly created iterator
 */
static void *vector_get_at_index_seq(cobalt_sequence_t *self, size_t index)
{
    cobalt_vector_impl_t *vec = (cobalt_vector_impl_t *)self;
    if (index >= vec->size) {
        return NULL;
    }
    return vec->items[index];
}

static cobalt_iterator_t *vector_iterator_seq(cobalt_sequence_t *self)
{
    cobalt_vector_impl_t *vec = (cobalt_vector_impl_t *)self;
    return cobalt_iterator_new((cobalt_sequence_t *)vec);
}

/* ========================================================================= */
/* Dynamic array (Vector) public API implementation                          */
/* ========================================================================= */

/**
 * @brief Create a new dynamic array
 */
cobalt_vector_t *cobalt_vector_create(size_t initial_capacity)
{
    cobalt_vector_impl_t *vec = malloc(sizeof(cobalt_vector_impl_t));
    if (!vec) {
        return NULL;
    }

    // Initialize internal array
    vec->items    = initial_capacity > 0 ? malloc(initial_capacity * sizeof(void *)) : NULL;
    vec->capacity = initial_capacity;
    vec->size     = 0;

    /* Initialize the method table for the sequence (Sequence) interface */
    vec->base.size         = vector_size_seq;
    vec->base.is_empty     = vector_is_empty_seq;
    vec->base.add          = vector_add_seq;
    vec->base.remove       = vector_remove_seq;
    vec->base.iterator     = vector_iterator_seq;
    vec->base.get_at_index = vector_get_at_index_seq;

    return (cobalt_vector_t *)vec;
}

/**
 * @brief Destroy a dynamic array
 */
void cobalt_vector_destroy(cobalt_vector_t *vec)
{
    if (vec) {
        cobalt_vector_impl_t *impl = (cobalt_vector_impl_t *)vec;
        free(impl->items);
        free(impl);
    }
}

/**
 * @brief Add an element to the end of the dynamic array
 */
int cobalt_vector_push(cobalt_vector_t *vec, void *item)
{
    if (!vec) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    // Reuse the sequence interface addition logic.
    // The cast is safe: cobalt_vector_impl_t and struct cobalt_vector
    // share the same cobalt_sequence_t base at offset 0.
    vector_add_seq((cobalt_sequence_t *)vec, item);
    return 0;
}

/**
 * @brief Get the element at the specified index
 */
void *cobalt_vector_get(const cobalt_vector_t *vec, size_t index)
{
    if (!vec) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    cobalt_vector_impl_t *impl = (cobalt_vector_impl_t *)vec;
    if (index >= impl->size) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_BOUNDS);
        return NULL;
    }
    return impl->items[index];
}

/**
 * @brief Set the element at the specified index
 */
int cobalt_vector_set(cobalt_vector_t *vec, size_t index, void *item)
{
    if (!vec || index >= ((cobalt_vector_impl_t *)vec)->size) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    ((cobalt_vector_impl_t *)vec)->items[index] = item;
    return 0;
}

/**
 * @brief Get the number of elements currently stored in the dynamic array
 */
size_t cobalt_vector_size(const cobalt_vector_t *vec)
{
    return vec ? ((cobalt_vector_impl_t *)vec)->size : 0;
}

/**
 * @brief Check if the dynamic array is empty
 */
int cobalt_vector_is_empty(const cobalt_vector_t *vec)
{
    return vec && ((cobalt_vector_impl_t *)vec)->size == 0;
}
