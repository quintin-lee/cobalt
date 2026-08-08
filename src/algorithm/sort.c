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

/*
 * @brief Perform quicksort
 *
 * A direct wrapper around the standard library's qsort function.
 */
void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    qsort(base, nmemb, size, compar);
}

/*
 * @brief Perform insertion sort
 *
 * Iterates through the elements in the array, inserting them into their correct positions in the
 * already sorted sub-sequence. For general data types, memcpy is used to copy and move elements.
 * Note: Since it requires temporarily caching an element, if malloc fails, the function will return
 * directly without sorting.
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
 */
void cobalt_list_sort(void **head, size_t *count, compare_func_t compar)
{
    if (!head || !compar) {
        return;
    }
    cobalt_list_node_t **list_head = (cobalt_list_node_t **)head;
    cobalt_list_merge_sort(list_head, count, compar);
}

void cobalt_stable_sort(void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    if (!base || nmemb <= 1) {
        return;
    }
    cobalt_insertion_sort(base, nmemb, size, compar);
}

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
