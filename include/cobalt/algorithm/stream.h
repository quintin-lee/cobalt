#ifndef COBALT_ALGORITHM_STREAM_H
#define COBALT_ALGORITHM_STREAM_H

/**
 * @file stream.h
 * @brief Stream processing utilities
 * @details Provides lazy-style stream operations: take_while, drop_while, and
 *          prefix extraction from arrays.
 */

#include "cobalt/algorithm/functional.h"
#include <stddef.h>

/**
 * @defgroup Stream_Module Stream Module
 * @{
 */

/**
 * @brief Extract the first n elements from an array
 * @param input Source array
 * @param output Destination array (must be at least n * size bytes)
 * @param n Number of elements to take
 * @param nmemb Total elements available in input
 * @param size Size of each element in bytes
 */
void cobalt_stream_take(const void *input, void *output, size_t n, size_t nmemb, size_t size);

/**
 * @brief Skip the first n elements and copy the rest
 * @param input Source array
 * @param output Destination array (must be at least (nmemb - n) * size bytes)
 * @param n Number of elements to skip
 * @param nmemb Total elements available in input
 * @param size Size of each element in bytes
 * @param out_nemb Pointer to store the number of elements written (NULL to skip)
 */
void cobalt_stream_drop(
    const void *input, void *output, size_t n, size_t nmemb, size_t size, size_t *out_nemb);

/**
 * @brief Copy elements while predicate returns true, stop at first false
 * @param input Source array
 * @param output Destination array
 * @param nmemb Input element count (updated with actual elements copied)
 * @param size Size of each element
 * @param pred Predicate function
 */
void cobalt_stream_take_while(
    const void *input, void *output, size_t *nmemb, size_t size, predicate_func_t pred);

/**
 * @brief Skip elements while predicate returns true, copy the rest
 * @param input Source array
 * @param output Destination array
 * @param nmemb Input element count (updated with actual elements copied)
 * @param size Size of each element
 * @param pred Predicate function
 */
void cobalt_stream_drop_while(
    const void *input, void *output, size_t *nmemb, size_t size, predicate_func_t pred);

/** @} */

#endif /* COBALT_ALGORITHM_STREAM_H */
