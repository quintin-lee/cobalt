#ifndef SORT_H
#define SORT_H

/**
 * @file sort.h
 * @brief General sorting algorithms
 * @details Provides declarations for general sorting algorithms within the framework, including array-based quicksort and insertion sort, as well as linked list-based merge sort.
 */

#include <stddef.h>

/**
 * @defgroup Sort_Module Sorting Module
 * @{
 */

/**
 * @brief Comparison function type definition
 * @param a Pointer to the first element to compare
 * @param b Pointer to the second element to compare
 * @return An integer representing the comparison result. Returns a value less than 0 if a < b, 0 if a == b, and greater than 0 if a > b.
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
 * @note Internally calls the standard library's qsort function, which is an unstable sorting algorithm.
 */
void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar);

/**
 * @brief Insertion sort
 * @param base Pointer to the first element of the array to be sorted
 * @param nmemb Number of elements in the array
 * @param size Size of each element in the array (in bytes)
 * @param compar Function used to compare two elements
 * @note This is a stable sorting algorithm. It is highly efficient when the number of elements is small or when the elements are mostly sorted. It allocates temporary space equal to the size of an element for data swapping internally.
 */
void cobalt_insertion_sort(void *base, size_t nmemb, size_t size, compare_func_t compar);

/* -----------------------------------------------------------------------------
 *  Linked list-based sorting
 * -------------------------------------------------------------------------- */

/**
 * @brief Linked list sort (using merge sort)
 * @param head Pointer to the pointer of the linked list head node
 * @param count Pointer to the number of nodes in the linked list (optional, can be used for optimization)
 * @param compar Function used to compare two elements
 * @note This is currently a placeholder interface; the actual linked list merge sort implementation is not yet complete.
 */
void cobalt_list_sort(void **head, size_t *count, compare_func_t compar);

/** @} */

#endif /* SORT_H */
