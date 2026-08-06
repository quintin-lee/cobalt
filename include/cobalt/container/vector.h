#ifndef VECTOR_H
#define VECTOR_H

/**
 * @file vector.h
 * @brief Dynamic array (Vector) container
 *
 * Provides a sequential container based on a dynamic array. It supports automatic expansion,
 * and implements the cobalt_sequence_t interface, allowing polymorphic sequence operations.
 */

#include "cobalt/interface/sequence.h"

/**
 * @brief Opaque type definition for the dynamic array structure.
 * @details The internal layout is defined in vector.c; users must interact
 *          with vectors only through the public API functions.
 */
typedef struct cobalt_vector cobalt_vector_t;

/**
 * @defgroup vector_api Dynamic array operation interfaces
 * @{
 */

/**
 * @brief Create a new dynamic array
 *
 * @param initial_capacity Initially allocated capacity. If 0, no initial array memory is allocated
 * by default.
 * @return Returns a pointer to the newly created dynamic array on success; returns NULL if memory
 * allocation fails.
 */
cobalt_vector_t *cobalt_vector_create(size_t initial_capacity);

/**
 * @brief Destroy a dynamic array and free its memory
 *
 * Frees the array memory inside the dynamic array as well as the structure itself.
 * @param vec Pointer to the dynamic array to be destroyed. If NULL, no action is taken.
 * @warning This operation only frees the memory of the container itself; it does not automatically
 * free the data pointed to by the element pointers stored in it.
 */
void cobalt_vector_destroy(cobalt_vector_t *vec);

/**
 * @brief Add an element to the end of the dynamic array
 *
 * @param vec Pointer to the target dynamic array
 * @param item Pointer to the element to be added
 * @return Returns 0 on success; returns -1 on failure (e.g., encountering a null pointer or memory
 * allocation failure).
 */
int cobalt_vector_push(cobalt_vector_t *vec, void *item);

/**
 * @brief Get the element at the specified index
 *
 * @param vec Pointer to the target dynamic array
 * @param index The index of the element to access (starting from 0)
 * @return Returns the element pointer at the specified position on success; returns NULL if vec is
 * empty or the index is out of bounds.
 */
void *cobalt_vector_get(const cobalt_vector_t *vec, size_t index);

/**
 * @brief Set the element at the specified index
 *
 * @param vec Pointer to the target dynamic array
 * @param index The index of the element to set (starting from 0)
 * @param item New element pointer
 * @return Returns 0 on success; if vec is empty or the index is out of bounds, returns -1 and sets
 * the corresponding error code.
 */
int cobalt_vector_set(cobalt_vector_t *vec, size_t index, void *item);

/**
 * @brief Get the number of elements currently stored in the dynamic array
 *
 * @param vec Pointer to the target dynamic array
 * @return The number of elements in the dynamic array. If vec is NULL, returns 0.
 */
size_t cobalt_vector_size(const cobalt_vector_t *vec);

/**
 * @brief Check if the dynamic array is empty
 *
 * @param vec Pointer to the target dynamic array
 * @return If the dynamic array is empty (i.e., size is 0), returns a non-zero value (1); otherwise
 * returns 0. If vec is NULL, also returns 0.
 */
int cobalt_vector_is_empty(const cobalt_vector_t *vec);

/** @} */

#endif /* VECTOR_H */
