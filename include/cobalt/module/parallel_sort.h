#ifndef COBALT_MODULE_PARALLEL_SORT_H
#define COBALT_MODULE_PARALLEL_SORT_H

/**
 * @file parallel_sort.h
 * @brief High-performance multi-core parallel sorting algorithms
 * @details Implements multi-core parallel pattern-defeating quicksort and adaptive stable sort
 *          leveraging work-stealing threadpool and Help-First synchronization barriers.
 *
 * @defgroup ParallelSort Parallel Sorting
 * @ingroup Modules
 * @{
 */

#include "cobalt/algorithm/sort.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/module/threadpool.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief In-place parallel pattern-defeating quicksort
 * @param pool Thread pool for task dispatch (falls back to sequential if NULL or 1 core)
 * @param base Pointer to array memory
 * @param nmemb Number of elements
 * @param size Element size in bytes
 * @param compar Element comparison function
 * @note Unstable sort, zero heap allocations, O(N log N) worst-case time complexity.
 */
void cobalt_parallel_qsort(
    cobalt_threadpool_t *pool, void *base, size_t nmemb, size_t size, compare_func_t compar);

/**
 * @brief In-place parallel quicksort with customizable sub-task cutoff threshold
 * @param pool Thread pool
 * @param base Pointer to array memory
 * @param nmemb Number of elements
 * @param size Element size in bytes
 * @param compar Element comparison function
 * @param cutoff Sub-array size threshold below which sorting switches to sequential PDQSort
 */
void cobalt_parallel_qsort_with_cutoff(cobalt_threadpool_t *pool,
                                       void                *base,
                                       size_t               nmemb,
                                       size_t               size,
                                       compare_func_t       compar,
                                       size_t               cutoff);

/**
 * @brief Parallel adaptive stable sort (TimSort-lite)
 * @param pool Thread pool for concurrent chunk sorting and hierarchical merging
 * @param base Pointer to array memory
 * @param nmemb Number of elements
 * @param size Element size in bytes
 * @param compar Element comparison function
 * @note 100% stable sorting guarantee.
 */
void cobalt_parallel_stable_sort(
    cobalt_threadpool_t *pool, void *base, size_t nmemb, size_t size, compare_func_t compar);

/**
 * @brief Parallel adaptive stable sort with custom allocator for scratch merge memory
 * @param pool Thread pool
 * @param base Pointer to array memory
 * @param nmemb Number of elements
 * @param size Element size in bytes
 * @param compar Element comparison function
 * @param alloc Custom memory allocator (falls back to system allocator if NULL)
 */
void cobalt_parallel_stable_sort_with_alloc(cobalt_threadpool_t *pool,
                                            void                *base,
                                            size_t               nmemb,
                                            size_t               size,
                                            compare_func_t       compar,
                                            cobalt_allocator_t  *alloc);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* COBALT_MODULE_PARALLEL_SORT_H */
