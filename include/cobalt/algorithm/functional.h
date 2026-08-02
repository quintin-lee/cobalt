#ifndef FUNCTIONAL_H
#define FUNCTIONAL_H

/**
 * @file functional.h
 * @brief Functional programming and algorithm helper interfaces
 * @details Defines predicates, operation function types, and common search and traversal algorithm declarations required by generic algorithms.
 */

#include "cobalt/algorithm/sort.h"
#include <stddef.h>

/**
 * @defgroup Functional_Module Functional and Algorithm Module
 * @{
 */

/**
 * @brief Predicate function type
 * @param item Pointer to the element to test
 * @return Returns non-zero (true) if the condition is met, otherwise returns 0 (false)
 */
typedef int (*predicate_func_t)(const void *item);

/**
 * @brief Generic operation function type
 * @param item Pointer to the element to process
 */
typedef void (*operation_func_t)(void *item);

/**
 * @brief Function object (used to store a callable entity and its context)
 */
typedef struct {
    void *context; /**< Additional context environment required for function execution */
    void (*apply)(void *context, const void *item); /**< Pointer to the function that actually executes the application */
} function_obj_t;

/* -----------------------------------------------------------------------------
 *  Common predicate functions
 * -------------------------------------------------------------------------- */

/**
 * @brief Check if two elements are equal
 * @param a Pointer to the first element
 * @param b Pointer to the second element
 * @param comp Function used for comparison
 * @return Non-zero if equal, otherwise 0
 */
int predicate_equal(const void *a, const void *b, compare_func_t comp);

/**
 * @brief Check if two elements are not equal
 * @param a Pointer to the first element
 * @param b Pointer to the second element
 * @param comp Function used for comparison
 * @return Non-zero if not equal, otherwise 0
 */
int predicate_not_equal(const void *a, const void *b, compare_func_t comp);

/**
 * @brief Check if a pointer is null
 * @param item Pointer to test
 * @return Non-zero if null, otherwise 0
 */
int predicate_null(const void *item);

/**
 * @brief Check if a pointer is non-null
 * @param item Pointer to test
 * @return Non-zero if non-null, otherwise 0
 */
int predicate_nonnull(const void *item);

/* -----------------------------------------------------------------------------
 *  Generic search and traversal algorithms
 * -------------------------------------------------------------------------- */

/**
 * @brief Perform a binary search in a sorted array
 * @param key Pointer to the target element to find
 * @param base Pointer to the first element of the array
 * @param nmemb Number of elements in the array
 * @param size Size of a single element (in bytes)
 * @param compar Function used to compare elements
 * @return Pointer to the element if found, otherwise NULL
 * @note The array must already be sorted in ascending order.
 */
void *
cobalt_bsearch(const void *key, const void *base, size_t nmemb, size_t size, compare_func_t compar);

/**
 * @brief Find the first element that matches the predicate
 * @param base Pointer to the first element of the array
 * @param nmemb Number of elements in the array
 * @param size Size of a single element (in bytes)
 * @param pred Predicate function used to test elements
 * @return Pointer to the first element that satisfies the predicate, or NULL if not found
 */
void *cobalt_find_if(const void *base, size_t nmemb, size_t size, predicate_func_t pred);

/**
 * @brief Apply the given operation function to each element of the array
 * @param base Pointer to the first element of the array
 * @param nmemb Number of elements in the array
 * @param size Size of a single element (in bytes)
 * @param op Operation function to call for each element
 */
void cobalt_for_each(const void *base, size_t nmemb, size_t size, operation_func_t op);

/** @} */

#endif /* FUNCTIONAL_H */
