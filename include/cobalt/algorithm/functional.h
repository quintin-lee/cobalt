#ifndef FUNCTIONAL_H
#define FUNCTIONAL_H

/**
 * @file functional.h
 * @generic functional utilities and predicates
 */

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

#endif /* FUNCTIONAL_H */
