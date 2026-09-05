/**
 * @file functional.c
 * @brief Implementation of functional programming and algorithm helper interfaces
 */
#include "cobalt/algorithm/functional.h"
#include <stddef.h>
#include <string.h>

/**
 * @brief Test two elements for equality via a comparator
 *
 * @param a First element, NULL compares equal only to NULL
 * @param b Second element
 * @param comp Comparator returning 0 for equal elements
 * @return Non-zero when equal, 0 otherwise
 */
int predicate_equal(const void *a, const void *b, compare_func_t comp)
{
    if (!a || !b) {
        return a == b;
    }
    return comp(a, b) == 0;
}

/**
 * @brief Test two elements for inequality via a comparator
 *
 * @param a First element
 * @param b Second element
 * @param comp Comparator returning 0 for equal elements
 * @return Non-zero when not equal, 0 otherwise
 */
int predicate_not_equal(const void *a, const void *b, compare_func_t comp)
{
    return !predicate_equal(a, b, comp);
}

/**
 * @brief Test whether an element pointer is NULL
 *
 * @param item Element pointer to test
 * @return Non-zero when item is NULL, 0 otherwise
 */
int predicate_null(const void *item)
{
    return item == NULL;
}

/**
 * @brief Test whether an element pointer is non-NULL
 *
 * @param item Element pointer to test
 * @return Non-zero when item is not NULL, 0 otherwise
 */
int predicate_nonnull(const void *item)
{
    return item != NULL;
}

/**
 * @brief Binary-search a sorted array for a key
 *
 * @param key Key to search for
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparator establishing the sort order
 * @return Pointer to the matching element, or NULL when not found or on invalid input
 */
void *
cobalt_bsearch(const void *key, const void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    if (!key || !base || nmemb == 0 || !compar) {
        return NULL;
    }

    const char *arr   = (const char *)base;
    size_t      left  = 0;
    size_t      right = nmemb;

    while (left < right) {
        size_t mid = left + (right - left) / 2;
        int    cmp = compar(key, arr + mid * size);

        if (cmp == 0) {
            return (void *)(arr + mid * size);
        }
        if (cmp < 0) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }

    return NULL;
}

/**
 * @brief Find the first element satisfying a predicate
 *
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param pred Predicate selecting the wanted element
 * @return Pointer to the first matching element, or NULL when none matches
 */
void *cobalt_find_if(const void *base, size_t nmemb, size_t size, predicate_func_t pred)
{
    if (!base || nmemb == 0 || !pred) {
        return NULL;
    }

    const char *arr = (const char *)base;
    for (size_t i = 0; i < nmemb; i++) {
        if (pred(arr + i * size)) {
            return (void *)(arr + i * size);
        }
    }

    return NULL;
}

/**
 * @brief Apply an operation to every array element in order
 *
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param op Operation applied to each element
 */
void cobalt_for_each(const void *base, size_t nmemb, size_t size, operation_func_t op)
{
    if (!base || nmemb == 0 || !op) {
        return;
    }

    const char *arr = (const char *)base;
    for (size_t i = 0; i < nmemb; i++) {
        op((void *)(arr + i * size));
    }
}

/**
 * @brief Transform each input element into the output array
 *
 * @param input Pointer to the first input element
 * @param output Pointer to the first output element
 * @param nmemb Number of elements to transform
 * @param size Size of each element in bytes
 * @param fn Mapping applied to each element pair
 * @param user_data Opaque context forwarded to fn
 * @return 0 on success, -1 on invalid input
 */
int cobalt_map(
    const void *input, void *output, size_t nmemb, size_t size, map_func_t fn, void *user_data)
{
    if (!input || !output || nmemb == 0 || !fn) {
        return -1;
    }

    const char *src = (const char *)input;
    char       *dst = (char *)output;
    for (size_t i = 0; i < nmemb; i++) {
        fn(src + i * size, dst + i * size, user_data);
    }
    return 0;
}

/**
 * @brief Copy elements satisfying a predicate, compacting in place-safe order
 *
 * @param input Pointer to the first input element
 * @param output Destination buffer (may overlap input for in-place filtering)
 * @param nmemb On entry the input count, on exit the number of kept elements
 * @param size Size of each element in bytes
 * @param pred Predicate selecting kept elements
 * @return 0 on success, -1 on invalid input
 */
int cobalt_filter(
    const void *input, void *output, size_t *nmemb, size_t size, predicate_func_t pred)
{
    if (!input || !output || !nmemb || !pred) {
        return -1;
    }

    const char *src   = (const char *)input;
    char       *dst   = (char *)output;
    size_t      count = 0;

    for (size_t i = 0; i < *nmemb; i++) {
        if (pred(src + i * size)) {
            if (count != i) {
                memcpy(dst + count * size, src + i * size, size);
            }
            count++;
        }
    }

    *nmemb = count;
    return 0;
}

/**
 * @brief Reduce array elements into a single accumulator value
 *
 * @param input Pointer to the first input element
 * @param nmemb Number of elements to fold
 * @param size Size of each element in bytes
 * @param initial Starting accumulator value (returned untouched on invalid input)
 * @param fn Folding function combining accumulator with each element
 * @param user_data Opaque context forwarded to fn
 * @return Final accumulator value
 */
void *cobalt_fold(
    const void *input, size_t nmemb, size_t size, void *initial, fold_func_t fn, void *user_data)
{
    if (!input || !fn || nmemb == 0) {
        return initial;
    }

    void       *accumulator = initial;
    const char *arr         = (const char *)input;

    for (size_t i = 0; i < nmemb; i++) {
        accumulator = fn(accumulator, arr + i * size, user_data);
    }

    return accumulator;
}
