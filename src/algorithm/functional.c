/**
 * @file functional.c
 * @brief Implementation of functional programming and algorithm helper interfaces
 */
#include "cobalt/algorithm/functional.h"
#include <stddef.h>
#include <string.h>

int predicate_equal(const void *a, const void *b, compare_func_t comp)
{
    if (!a || !b) {
        return a == b;
    }
    return comp(a, b) == 0;
}

int predicate_not_equal(const void *a, const void *b, compare_func_t comp)
{
    return !predicate_equal(a, b, comp);
}

int predicate_null(const void *item)
{
    return item == NULL;
}

int predicate_nonnull(const void *item)
{
    return item != NULL;
}

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
