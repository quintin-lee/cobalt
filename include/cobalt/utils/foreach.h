#ifndef COBALT_FOREACH_H
#define COBALT_FOREACH_H

/**
 * @file foreach.h
 * @brief Range-based for-loop macros for C11
 *
 * Provides cobalt_foreach and cobalt_foreach_rev macros that work with
 * cobalt containers implementing the cobalt_sequence_t interface.
 */

#include "cobalt/interface/iterator.h"
#include "cobalt/interface/sequence.h"

/**
 * @brief Iterate over a sequence from beginning to end
 * @param var  Variable name to hold each element (use pointer type)
 * @param seq  cobalt_sequence_t* (or any type that casts to it)
 */
#define cobalt_foreach(var, seq)                                                                   \
    for (cobalt_iterator_t *_foreach_iter = cobalt_iterator_new((cobalt_sequence_t *)(seq));       \
         _foreach_iter != NULL &&                                                                  \
         (var = cobalt_iterator_next(_foreach_iter), _foreach_iter != NULL);                       \
         cobalt_iterator_destroy(_foreach_iter), _foreach_iter = NULL)

/**
 * @brief Iterate over a vector in reverse order
 * @param var   Variable name to hold each element
 * @param seq   cobalt_vector_t*
 */
#define cobalt_foreach_rev(var, seq)                                                               \
    for (size_t _foreach_idx = cobalt_vector_size((cobalt_vector_t *)(seq)); _foreach_idx > 0;     \
         --_foreach_idx)                                                                           \
        for (var = cobalt_vector_get((cobalt_vector_t *)(seq), _foreach_idx - 1); var != NULL;     \
             var = (void *)0)

#endif /* COBALT_FOREACH_H */
