#ifndef SORT_H
#define SORT_H

/**
 * @file sort.h
 * @generic sorting algorithms
 */

#include <stddef.h>

/* Comparison function type */
typedef int (*compare_func_t)(const void *a, const void *b);

/* Array-based sorting */
void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar);
void cobalt_insertion_sort(void *base, size_t nmemb, size_t size, compare_func_t compar);

/* List-based sorting (using merge sort on linked lists) */
void cobalt_list_sort(void **head, size_t *count, compare_func_t compar);

#endif /* SORT_H */
