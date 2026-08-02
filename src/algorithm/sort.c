/**
 * @file sort.c
 * @brief Implementation of general sorting algorithms
 */
#include "cobalt/algorithm/sort.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

/*
 * @brief Sort a linked list
 *
 * Plans to use merge sort to sort the linked list. Currently an unimplemented placeholder.
 */
void cobalt_list_sort(void **head, size_t *count, compare_func_t compar)
{
    // Avoid unused parameter warnings
    (void)head;
    (void)count;
    (void)compar;
    /* Merge sort on linked list - placeholder */
}
