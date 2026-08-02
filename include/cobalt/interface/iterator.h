#ifndef ITERATOR_H
#define ITERATOR_H

/**
 * @file iterator.h
 * @brief Iterator interface definition
 *
 * Defines the generic iterator interface and related virtual function table for traversing collections (such as sequences, maps, etc.).
 */

#include "sequence.h"
#include <stddef.h>

/**
 * @defgroup Iterator Iterator Module
 * @{
 */

/**
 * @brief Iterator virtual function table
 * @details A collection of function pointers used to implement polymorphism. Specific iterators need to implement these methods.
 */
typedef struct cobalt_iterator_vtable {
    /**
     * @brief Check if there is a next element
     * @param iter The specific iterator context pointer
     * @return Non-zero (1) if there is a next element, 0 otherwise
     */
    int (*has_next)(void *iter);

    /**
     * @brief Get the next element
     * @param iter The specific iterator context pointer
     * @return A pointer to the next element, or NULL if there is none
     */
    void *(*next)(void *iter);

    /**
     * @brief Destroy the iterator context
     * @param iter The specific iterator context pointer
     */
    void (*destroy)(void *iter);
} cobalt_iterator_vtable_t;

/**
 * @brief Iterator abstract type
 */
typedef struct cobalt_iterator cobalt_iterator_t;

/**
 * @brief Iterator structure definition
 */
struct cobalt_iterator {
    const cobalt_iterator_vtable_t *vtable; /**< Virtual function table pointer, pointing to specific implementation methods */
    void                           *data;   /**< Iterator private data/context */
};

/**
 * @brief Create a new iterator for a sequence
 * @param seq Pointer to the sequence to be traversed
 * @return Returns a pointer to the newly created iterator on success, or NULL on failure
 * @note The returned iterator needs to be freed using cobalt_iterator_destroy()
 */
cobalt_iterator_t *cobalt_iterator_new(cobalt_sequence_t *seq);

/**
 * @brief Check if the iterator has a next element
 * @param iter Iterator pointer
 * @return 1 if there is a next element, 0 otherwise
 */
int                cobalt_iterator_has_next(cobalt_iterator_t *iter);

/**
 * @brief Get the next element from the iterator
 * @param iter Iterator pointer
 * @return A pointer to the next element, or NULL if the end has been reached
 */
void              *cobalt_iterator_next(cobalt_iterator_t *iter);

/**
 * @brief Destroy and free iterator resources
 * @param iter Pointer to the iterator to be destroyed
 */
void               cobalt_iterator_destroy(cobalt_iterator_t *iter);

/** @} */

#endif /* ITERATOR_H */
