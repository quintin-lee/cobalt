#ifndef SORT_H
#define SORT_H

/**
 * @file sort.h
 * @brief General sorting algorithms
 * @details Provides declarations for general sorting algorithms within the framework, including
 * array-based quicksort and insertion sort, as well as linked list-based merge sort.
 */

#include "cobalt/memory/allocator.h"
#include <stddef.h>

/**
 * @defgroup Sort_Module Sorting Module
 * @{
 */

/**
 * @brief Comparison function type definition
 * @param a Pointer to the first element to compare
 * @param b Pointer to the second element to compare
 * @return An integer representing the comparison result. Returns a value less than 0 if a < b, 0 if
 * a == b, and greater than 0 if a > b.
 */
typedef int (*compare_func_t)(const void *a, const void *b);

/* -----------------------------------------------------------------------------
 *  Array-based sorting
 * -------------------------------------------------------------------------- */

/**
 * @brief Quicksort
 * @param base Pointer to the first element of the array to be sorted
 * @param nmemb Number of elements in the array
 * @param size Size of each element in the array (in bytes)
 * @param compar Function used to compare two elements
 * @note Internally calls the standard library's qsort function, which is an unstable sorting
 * algorithm.
 */
void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar);

/**
 * @brief Insertion sort
 * @param base Pointer to the first element of the array to be sorted
 * @param nmemb Number of elements in the array
 * @param size Size of each element in the array (in bytes)
 * @param compar Function used to compare two elements
 * @note This is a stable sorting algorithm. It is highly efficient when the number of elements is
 * small or when the elements are mostly sorted. It allocates temporary space equal to the size of
 * an element for data swapping internally.
 */
void cobalt_insertion_sort(void *base, size_t nmemb, size_t size, compare_func_t compar);

/**
 * @brief Insertion sort with custom allocator
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparator establishing the sort order
 * @param alloc Custom allocator (NULL falls back to system)
 * @note Same behavior as cobalt_insertion_sort but with injectable allocator
 */
void cobalt_insertion_sort_with_alloc(
    void *base, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc);

/* -----------------------------------------------------------------------------
 *  Linked list-based sorting
 * -------------------------------------------------------------------------- */

/**
 * @brief Linked list sort (using merge sort)
 * @param head Pointer to the pointer of the linked list head node
 * @param count Pointer to the number of nodes in the linked list (optional, can be used for
 * optimization)
 * @param compar Function used to compare two elements
 * @note This is a stable O(n log n) merge sort. The list is modified in-place.
 */
void cobalt_list_sort(void **head, size_t *count, compare_func_t compar);

/* -----------------------------------------------------------------------------
 *  Additional sorting and sequence utilities
 * -------------------------------------------------------------------------- */

/**
 * @brief Stable sort (uses insertion sort internally)
 * @param base Pointer to the first element of the array to be sorted
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparison function
 * @note This is a stable sorting algorithm — equal elements preserve their relative order.
 */
void cobalt_stable_sort(void *base, size_t nmemb, size_t size, compare_func_t compar);

/**
 * @brief Stable sort with custom allocator
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparison function
 * @param alloc Custom allocator (NULL falls back to system)
 * @note Same behavior as cobalt_stable_sort but with injectable allocator
 */
void cobalt_stable_sort_with_alloc(
    void *base, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc);

/**
 * @brief Partition array around a pivot value
 * @param base Pointer to the first element
 * @param nmemb Number of elements
 * @param size Element size in bytes
 * @param pivot Value to partition around
 * @param compar Comparison function
 * @return Index of the first element in the "greater-or-equal" partition
 * @note After partitioning, all elements < pivot are before the returned index,
 *       and all elements >= pivot are at or after it. Order within partitions is undefined.
 */
size_t
cobalt_partition(void *base, size_t nmemb, size_t size, const void *pivot, compare_func_t compar);

/**
 * @brief Partition array around a pivot using a custom allocator
 * @param base Pointer to the first element
 * @param nmemb Number of elements
 * @param size Element size in bytes
 * @param pivot Value to partition around
 * @param compar Comparison function
 * @param alloc Custom allocator (NULL falls back to system)
 * @return Index of the first element in the "greater-or-equal" partition
 * @note Same behavior as cobalt_partition but with injectable allocator
 */
size_t cobalt_partition_with_alloc(void               *base,
                                   size_t              nmemb,
                                   size_t              size,
                                   const void         *pivot,
                                   compare_func_t      compar,
                                   cobalt_allocator_t *alloc);

/**
 * @brief Remove consecutive duplicate elements from a sorted range
 * @param base Pointer to the first element
 * @param nmemb Input number of elements (updated to output count)
 * @param size Element size in bytes
 * @param compar Comparison function
 * @return New logical size after deduplication
 * @note The array must be sorted according to compar before calling.
 *       Elements after the returned count are unspecified.
 */
size_t cobalt_unique(void *base, size_t *nmemb, size_t size, compare_func_t compar);

/** @} */

#endif /* SORT_H */
