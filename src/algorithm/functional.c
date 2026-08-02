/**
 * @file functional.c
 * @brief Implementation of functional programming and algorithm helper interfaces
 */
#include "cobalt/algorithm/functional.h"
#include <stddef.h>

/*
 * @brief Check if two elements are equal
 * 
 * Combined with a null pointer check, if either a or b is NULL, it falls back to direct pointer comparison;
 * otherwise, it calls the provided comparison function comp to compare.
 */
int predicate_equal(const void *a, const void *b, compare_func_t comp)
{
    if (!a || !b) {
        return a == b;
    }
    return comp(a, b) == 0;
}

/*
 * @brief Check if two elements are not equal
 * 
 * Based on predicate_equal to implement logical negation.
 */
int predicate_not_equal(const void *a, const void *b, compare_func_t comp)
{
    return !predicate_equal(a, b, comp);
}

/*
 * @brief Predicate to check if a pointer is NULL
 */
int predicate_null(const void *item)
{
    return item == NULL;
}

/*
 * @brief Predicate to check if a pointer is not NULL
 */
int predicate_nonnull(const void *item)
{
    return item != NULL;
}

/*
 * @brief Binary search implementation to find a target key in a sorted array
 * 
 * Uses a method of continuously halving the partition to reduce the search range by half each time.
 * The array must be sorted in ascending order beforehand.
 */
void *
cobalt_bsearch(const void *key, const void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    // Parameter check: if any required parameter is missing, return NULL directly
    if (!key || !base || nmemb == 0 || !compar) {
        return NULL;
    }

    const char *arr   = (const char *)base;
    size_t      left  = 0;
    size_t      right = nmemb; // Use open interval as upper bound

    // Continuously halve the search range
    while (left < right) {
        size_t mid = left + (right - left) / 2; // Midpoint calculation to prevent overflow
        int    cmp = compar(key, arr + mid * size);

        if (cmp == 0) {
            // Target match found, return corresponding address
            return (void *)(arr + mid * size);
        } else if (cmp < 0) {
            // Target is less than midpoint, reduce right bound
            right = mid;
        } else {
            // Target is greater than midpoint, advance left bound
            left = mid + 1;
        }
    }

    // Matching element not found
    return NULL;
}

/*
 * @brief Linear search for the first element satisfying the specified condition
 * 
 * Iterates through each element in the array and returns the pointer to the first element that causes the pred predicate function to return true (non-zero).
 */
void *cobalt_find_if(const void *base, size_t nmemb, size_t size, predicate_func_t pred)
{
    if (!base || nmemb == 0 || !pred) {
        return NULL;
    }

    const char *arr = (const char *)base;
    for (size_t i = 0; i < nmemb; i++) {
        // Call the predicate function to test the current element
        if (pred(arr + i * size)) {
            return (void *)(arr + i * size);
        }
    }

    return NULL;
}

/*
 * @brief Execute a uniform operation on all elements in the array
 * 
 * Iterates through every element in the array and executes the passed op function on it.
 */
void cobalt_for_each(const void *base, size_t nmemb, size_t size, operation_func_t op)
{
    if (!base || nmemb == 0 || !op) {
        return;
    }

    const char *arr = (const char *)base;
    for (size_t i = 0; i < nmemb; i++) {
        // Cast read-only memory to read-write and execute the specified modification operation
        op((void *)(arr + i * size));
    }
}
