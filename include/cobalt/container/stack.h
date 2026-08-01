#ifndef STACK_H
#define STACK_H

/**
 * @file stack.h
 * @brief Stack (LIFO) container
 */

#include <stddef.h>

typedef struct cobalt_stack cobalt_stack_t;

/* Create a new stack */
cobalt_stack_t *cobalt_stack_create(void);

/* Destroy the stack */
void cobalt_stack_destroy(cobalt_stack_t *stack);

/* Push item onto stack */
int cobalt_stack_push(cobalt_stack_t *stack, void *item);

/* Pop item from stack */
void *cobalt_stack_pop(cobalt_stack_t *stack);

/* Peek at top item */
void *cobalt_stack_peek(cobalt_stack_t *stack);

/* Get stack size */
size_t cobalt_stack_size(cobalt_stack_t *stack);

/* Check if stack is empty */
int cobalt_stack_is_empty(cobalt_stack_t *stack);

#endif /* STACK_H */
