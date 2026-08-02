/**
 * @file iterator.c
 * @brief Iterator interface implementation
 *
 * Provides a generic implementation of a sequence iterator and standard methods for operating iterators.
 */

#include "cobalt/interface/iterator.h"
#include "cobalt/interface/sequence.h"
#include <stdlib.h>

/**
 * @brief Generic iterator context implementation for sequences
 * @details Maintains state information when traversing a sequence
 */
typedef struct {
    cobalt_sequence_t *seq;   /**< Sequence instance being traversed */
    size_t             index; /**< Currently traversed index */
    size_t             total; /**< Total number of elements in the sequence */
} cobalt_iterator_impl_t;

/* --- Generic iterator virtual function table implementation --- */

/**
 * @brief Check if the generic iterator has a next element
 * @param ctx Iterator context (cobalt_iterator_impl_t pointer)
 * @return 1 if the current index is less than the total, 0 otherwise
 */
static int generic_has_next(void *ctx)
{
    cobalt_iterator_impl_t *impl = (cobalt_iterator_impl_t *)ctx;
    return impl->index < impl->total;
}

/**
 * @brief Get the next element of the generic iterator
 * @param ctx Iterator context (cobalt_iterator_impl_t pointer)
 * @return The current implementation does not support directly returning an element pointer, always returns NULL, but advances the index
 * @note This provides a skeleton implementation, actual containers should provide their own specific iterator implementation
 */
static void *generic_next(void *ctx)
{
    cobalt_iterator_impl_t *impl = (cobalt_iterator_impl_t *)ctx;
    if (impl->index >= impl->total) {
        return NULL; /* End reached */
    }
    impl->index++; /* Advance iterator state */
    return NULL; /* The generic iterator does not support direct element access, it needs to be overridden by a specific sequence implementation */
}

/**
 * @brief Destroy the generic iterator context
 * @param ctx Pointer to the context to free
 */
static void generic_destroy(void *ctx)
{
    if (ctx) {
        free(ctx);
    }
}

/**
 * @brief Generic iterator virtual function table instance
 */
static const cobalt_iterator_vtable_t generic_vtable = {
    .has_next = generic_has_next, .next = generic_next, .destroy = generic_destroy};

/* --- Iterator public API implementation --- */

/**
 * @brief Create a generic iterator for the specified sequence
 * @param seq Target sequence
 * @return The created iterator instance, or NULL if memory allocation fails
 */
cobalt_iterator_t *cobalt_iterator_new(cobalt_sequence_t *seq)
{
    if (!seq) {
        return NULL; /* Defensive check: sequence cannot be null */
    }

    // Allocate iterator shell structure
    cobalt_iterator_t *iter = malloc(sizeof(cobalt_iterator_t));
    if (!iter) {
        return NULL;
    }

    // Allocate iterator internal state implementation
    cobalt_iterator_impl_t *impl = malloc(sizeof(cobalt_iterator_impl_t));
    if (!impl) {
        free(iter);
        return NULL;
    }

    // Initialize state
    impl->seq   = seq;
    impl->index = 0;
    impl->total = seq->size(seq);

    // Bind virtual function table and data
    iter->vtable = &generic_vtable;
    iter->data   = impl;
    return iter;
}

/**
 * @brief Check if there is a next element
 * @details Delegates to the has_next method in the virtual function table
 */
int cobalt_iterator_has_next(cobalt_iterator_t *iter)
{
    if (!iter || !iter->vtable) {
        return 0; /* Invalid iterator is considered end of traversal */
    }
    return iter->vtable->has_next(iter->data);
}

/**
 * @brief Get the next element
 * @details Delegates to the next method in the virtual function table
 */
void *cobalt_iterator_next(cobalt_iterator_t *iter)
{
    if (!iter || !iter->vtable) {
        return NULL;
    }
    return iter->vtable->next(iter->data);
}

/**
 * @brief Destroy the iterator
 * @details First calls the virtual function table to destroy the internal context, then frees the iterator shell
 */
void cobalt_iterator_destroy(cobalt_iterator_t *iter)
{
    if (!iter) {
        return;
    }
    // Destroy internal implementation data
    if (iter->vtable && iter->vtable->destroy) {
        iter->vtable->destroy(iter->data);
    }
    // Free the iterator itself
    free(iter);
}
