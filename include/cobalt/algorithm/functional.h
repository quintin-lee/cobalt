#ifndef FUNCTIONAL_H
#define FUNCTIONAL_H

/**
 * @file functional.h
 * @brief Functional programming and algorithm helper interfaces
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
 * @brief Map function type - transforms each element
 * @param item Pointer to the input element
 * @param output Pointer to where the output should be written
 * @param user_data Optional user data passed through
 */
typedef void (*map_func_t)(const void *item, void *output, void *user_data);

/**
 * @brief Fold/Reduce function type - accumulates results
 * @param accumulator Current accumulated value
 * @param item Current element
 * @param user_data Optional user data passed through
 * @return New accumulated value
 */
typedef void *(*fold_func_t)(void *accumulator, const void *item, void *user_data);

/* -----------------------------------------------------------------------------
 *  Common predicate functions
 * -------------------------------------------------------------------------- */

int predicate_equal(const void *a, const void *b, compare_func_t comp);
int predicate_not_equal(const void *a, const void *b, compare_func_t comp);
int predicate_null(const void *item);
int predicate_nonnull(const void *item);

/* -----------------------------------------------------------------------------
 *  Generic search and traversal algorithms
 * -------------------------------------------------------------------------- */

void *cobalt_bsearch(const void *key, const void *base, size_t nmemb, size_t size, compare_func_t compar);
void *cobalt_find_if(const void *base, size_t nmemb, size_t size, predicate_func_t pred);
void cobalt_for_each(const void *base, size_t nmemb, size_t size, operation_func_t op);

/* -----------------------------------------------------------------------------
 *  Stream operators (map, filter, fold)
 * -------------------------------------------------------------------------- */

/**
 * @brief Apply a transformation function to each element and store results
 * @param input Array of input elements
 * @param output Array to store transformed elements
 * @param nmemb Number of elements
 * @param size Size of each element
 * @param fn Transform function
 * @param user_data Optional data passed to transform function
 * @return 0 on success, -1 on failure
 */
int cobalt_map(const void *input, void *output, size_t nmemb, size_t size, map_func_t fn, void *user_data);

/**
 * @brief Filter elements based on a predicate
 * @param input Array of input elements
 * @param output Array to store filtered elements
 * @param nmemb Input element count (updated with output count)
 * @param size Size of each element
 * @param pred Filter predicate
 * @return 0 on success, -1 on failure
 */
int cobalt_filter(const void *input, void *output, size_t *nmemb, size_t size, predicate_func_t pred);

/**
 * @brief Fold/reduce array to a single value
 * @param input Array of input elements
 * @param nmemb Number of elements
 * @param size Size of each element
 * @param initial Initial accumulator value
 * @param fn Fold function
 * @param user_data Optional data passed to fold function
 * @return Accumulated result (may be same as initial)
 */
void *cobalt_fold(const void *input, size_t nmemb, size_t size, void *initial, fold_func_t fn, void *user_data);

/** @} */

#endif /* FUNCTIONAL_H */
