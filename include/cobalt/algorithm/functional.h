#ifndef FUNCTIONAL_H
#define FUNCTIONAL_H

#include "cobalt/algorithm/sort.h"
#include <stddef.h>

/* Predicate function type */
typedef int (*predicate_func_t)(const void *item);

/* Generic operation types */
typedef void (*operation_func_t)(void *item);

/* Function object (for storing callable entities) */
typedef struct {
    void *context;
    void (*apply)(void *context, const void *item);
} function_obj_t;

/* Common predicates */
int predicate_equal(const void *a, const void *b, compare_func_t comp);
int predicate_not_equal(const void *a, const void *b, compare_func_t comp);
int predicate_null(const void *item);
int predicate_nonnull(const void *item);

/* Binary search on sorted array */
void *
cobalt_bsearch(const void *key, const void *base, size_t nmemb, size_t size, compare_func_t compar);

/* Find first element matching predicate */
void *cobalt_find_if(const void *base, size_t nmemb, size_t size, predicate_func_t pred);

/* Apply operation to each element */
void cobalt_for_each(const void *base, size_t nmemb, size_t size, operation_func_t op);

#endif /* FUNCTIONAL_H */
