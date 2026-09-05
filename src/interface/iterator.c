/**
 * @file iterator.c
 * @brief Iterator interface implementation
 *
 * Provides a generic implementation of a sequence iterator and standard methods for operating
 * iterators.
 */

#include "cobalt/interface/iterator.h"
#include "cobalt/interface/sequence.h"
#include "cobalt/memory/allocator.h"
#include <stdlib.h>

/**
 * @brief Generic iterator context implementation for sequences
 * @details Maintains state information when traversing a sequence
 */
typedef struct {
    cobalt_sequence_t  *seq;   /**< Sequence instance being traversed */
    size_t              index; /**< Currently traversed index */
    size_t              total; /**< Total number of elements in the sequence */
    cobalt_allocator_t *alloc; /**< Allocator that owns this context and the shell */
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
 * @return The next element pointer, or NULL if at end or sequence does not support get_at_index
 */
static void *generic_next(void *ctx)
{
    cobalt_iterator_impl_t *impl = (cobalt_iterator_impl_t *)ctx;
    if (impl->index >= impl->total) {
        return NULL; /* End reached */
    }
    void *item = impl->seq->get_at_index(impl->seq, impl->index);
    impl->index++;
    return item;
}

/**
 * @brief Destroy the generic iterator context
 * @param ctx Pointer to the context to free
 */
static void generic_destroy(void *ctx)
{
    if (ctx) {
        cobalt_iterator_impl_t *impl  = (cobalt_iterator_impl_t *)ctx;
        cobalt_allocator_t     *alloc = impl->alloc ? impl->alloc : cobalt_allocator_get_system();
        alloc->free(alloc, ctx);
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
    return cobalt_iterator_new_with_allocator(seq, cobalt_allocator_get_system());
}

/**
 * @brief Create a generic iterator for the specified sequence with a custom allocator
 * @details The allocator is stored in the internal context; cobalt_iterator_destroy() releases
 *          both the context and the shell through it automatically.
 * @param seq Target sequence
 * @param alloc Custom allocator, or NULL to fall back to the system allocator
 * @return The created iterator instance, or NULL if memory allocation fails
 */
cobalt_iterator_t *cobalt_iterator_new_with_allocator(cobalt_sequence_t  *seq,
                                                      cobalt_allocator_t *alloc)
{
    if (!seq) {
        return NULL; /* Defensive check: sequence cannot be null */
    }
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }

    // Allocate iterator shell structure
    cobalt_iterator_t *iter = alloc->alloc(alloc, sizeof(cobalt_iterator_t));
    if (!iter) {
        return NULL;
    }

    // Allocate iterator internal state implementation
    cobalt_iterator_impl_t *impl = alloc->alloc(alloc, sizeof(cobalt_iterator_impl_t));
    if (!impl) {
        alloc->free(alloc, iter);
        return NULL;
    }

    // Initialize state
    impl->seq   = seq;
    impl->index = 0;
    impl->total = seq->size(seq);
    impl->alloc = alloc;

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
 * @details First calls the virtual function table to destroy the internal context, then frees the
 * iterator shell
 */
void cobalt_iterator_destroy(cobalt_iterator_t *iter)
{
    if (!iter) {
        return;
    }
    /* Route the shell through the creating allocator for generic iterators; foreign vtables
       own an opaque context we must not interpret, so keep the legacy path for them. */
    cobalt_allocator_t *alloc = cobalt_allocator_get_system();
    if (iter->vtable == &generic_vtable && iter->data) {
        cobalt_iterator_impl_t *impl = (cobalt_iterator_impl_t *)iter->data;
        if (impl->alloc) {
            alloc = impl->alloc;
        }
    }
    // Destroy internal implementation data
    if (iter->vtable && iter->vtable->destroy) {
        iter->vtable->destroy(iter->data);
    }
    // Free the iterator itself
    alloc->free(alloc, iter);
}
