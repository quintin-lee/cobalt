#ifndef SEQUENCE_H
#define SEQUENCE_H

/**
 * @file sequence.h
 * @brief Sequence (ordered collection) interface
 *
 * Defines a generic abstract interface for collections with ordered characteristics (such as lists, vectors, etc.).
 */

#include <stddef.h>

/**
 * @defgroup Sequence Sequence Interface Module
 * @{
 */

/**
 * @brief Sequence abstract type
 */
typedef struct cobalt_sequence cobalt_sequence_t;

/**
 * @brief Iterator abstract type forward declaration
 */
typedef struct cobalt_iterator cobalt_iterator_t;

/**
 * @brief Sequence structure definition (interface polymorphism)
 * @details Defines standard operations such as size query, element addition/removal, and iterator retrieval
 */
struct cobalt_sequence {
    /**
     * @brief Get the number of elements in the sequence
     * @param self Sequence instance pointer
     * @return The number of elements in the sequence
     */
    size_t (*size)(cobalt_sequence_t *self);

    /**
     * @brief Check if the sequence is empty
     * @param self Sequence instance pointer
     * @return Non-zero (1) if the sequence is empty, 0 otherwise
     */
    int (*is_empty)(cobalt_sequence_t *self);

    /**
     * @brief Add an element to the sequence
     * @param self Sequence instance pointer
     * @param item Pointer to the element to add
     */
    void (*add)(cobalt_sequence_t *self, void *item);

    /**
     * @brief Remove a specified element from the sequence
     * @param self Sequence instance pointer
     * @param item Pointer to the element to remove
     */
    void (*remove)(cobalt_sequence_t *self, void *item);

    /**
     * @brief Get an iterator for traversing this sequence
     * @param self Sequence instance pointer
     * @return Returns a pointer to the iterator instance on success, or NULL on failure
     */
    cobalt_iterator_t *(*iterator)(cobalt_sequence_t *self);
};

/**
 * @brief Create a sequence instance
 * @param initial_capacity The initial capacity of the sequence
 * @return Returns a pointer to the newly created sequence on success, or NULL on failure
 * @note Memory needs to be freed using cobalt_sequence_destroy()
 */
cobalt_sequence_t *cobalt_sequence_create(size_t initial_capacity);

/**
 * @brief Destroy a sequence instance
 * @param seq Pointer to the sequence to be destroyed
 */
void cobalt_sequence_destroy(cobalt_sequence_t *seq);

/** @} */

#endif /* SEQUENCE_H */
