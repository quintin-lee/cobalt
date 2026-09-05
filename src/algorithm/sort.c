/**
 * @file sort.c
 * @brief Implementation of general sorting algorithms
 */
#include "cobalt/algorithm/sort.h"
#include "cobalt/container/list.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
/* compare_func_t is already defined in sort.h */

/**
 * @brief Perform quicksort via the standard library qsort
 *
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparator establishing the sort order
 * @note Unstable: equal elements may change relative order
 */
void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    qsort(base, nmemb, size, compar);
}

/**
 * @brief Perform insertion sort
 *
 * @details Iterates through the elements, inserting each into its correct position in the
 *          already sorted sub-sequence. Efficient for small or nearly sorted arrays.
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparator establishing the sort order
 * @note Requires a temporary element-size buffer; silently skips sorting when allocation fails
 */
void cobalt_insertion_sort(void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    // Handle null pointers or arrays with 1 or fewer elements, no sorting required
    if (!base || nmemb <= 1) {
        return;
    }

    char *arr = (char *)base;
    // Allocate temporary space the size of an element to cache the "reference key"
    char *key = malloc(size);
    if (!key) {
        return; // Memory allocation failed, return directly
    }

    // Start from the second element and insert into the sorted sequence in front
    for (size_t i = 1; i < nmemb; i++) {
        // Cache the current element to insert
        memcpy(key, arr + i * size, size);
        int j = (int)i - 1;

        // If the preceding element is greater than the key, move the preceding element one position
        // back
        while (j >= 0 && compar(arr + j * size, key) > 0) {
            memcpy(arr + (j + 1) * size, arr + j * size, size);
            j--;
        }
        // Insert the key into the appropriate position found
        memcpy(arr + (j + 1) * size, key, size);
    }

    // Free the temporary space
    free(key);
}

/**
 * @brief Sort a linked list using merge sort (stable, O(n log n))
 * @details Delegates to cobalt_list_merge_sort() in list.c
 *
 * @param head Pointer to the list head pointer (updated in place)
 * @param count Optional node count used for optimization, may be NULL
 * @param compar Comparator establishing the sort order
 */
void cobalt_list_sort(void **head, size_t *count, compare_func_t compar)
{
    if (!head || !compar) {
        return;
    }
    cobalt_list_node_t **list_head = (cobalt_list_node_t **)head;
    cobalt_list_merge_sort(list_head, count, compar);
}

/**
 * @brief Perform a stable sort via insertion sort
 *
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparator establishing the sort order
 * @note Stable: equal elements preserve their relative order
 */
void cobalt_stable_sort(void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    if (!base || nmemb <= 1) {
        return;
    }
    cobalt_insertion_sort(base, nmemb, size, compar);
}

/**
 * @brief Partition an array around a pivot value (Lomuto-style)
 *
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param pivot Pivot value that elements are compared against
 * @param compar Comparator establishing the element order
 * @return Index of the first element in the greater-or-equal partition, 0 on invalid input
 * @note Requires a temporary element-size buffer; returns 0 without partitioning when it fails
 */
size_t
cobalt_partition(void *base, size_t nmemb, size_t size, const void *pivot, compare_func_t compar)
{
    if (!base || nmemb == 0 || !compar || !pivot) {
        return 0;
    }
    char  *arr   = (char *)base;
    size_t left  = 0;
    size_t right = nmemb;
    char  *tmp   = (char *)malloc(size);
    if (!tmp) {
        return 0;
    }

    while (left < right) {
        if (compar(arr + left * size, pivot) < 0) {
            left++;
        } else {
            right--;
            if (left != right) {
                memcpy(tmp, arr + left * size, size);
                memcpy(arr + left * size, arr + right * size, size);
                memcpy(arr + right * size, tmp, size);
            }
        }
    }
    free(tmp);
    return left;
}

/**
 * @brief Remove consecutive duplicates from a sorted array in place
 *
 * @param base Pointer to the first array element
 * @param nmemb On entry the input count, on exit the deduplicated count
 * @param size Size of each element in bytes
 * @param compar Comparator establishing element equality
 * @return New logical size after deduplication
 */
size_t cobalt_unique(void *base, size_t *nmemb, size_t size, compare_func_t compar)
{
    if (!base || !nmemb || nmemb == NULL || *nmemb <= 1 || !compar) {
        return nmemb ? *nmemb : 0;
    }
    size_t count = *nmemb;
    size_t write = 1;
    char  *arr   = (char *)base;
    for (size_t i = 1; i < count; i++) {
        if (compar(arr + i * size, arr + (i - 1) * size) != 0) {
            if (write != i) {
                memcpy(arr + write * size, arr + i * size, size);
            }
            write++;
        }
    }
    *nmemb = write;
    return write;
}
