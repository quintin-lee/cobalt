#ifndef STACK_H
#define STACK_H

/**
 * @file stack.h
 * @brief Stack (Last-In, First-Out, LIFO) container
 * @details Provides a generic stack data structure implementation, based on a singly linked list, storing void* type pointer data.
 */

#include <stddef.h>

/**
 * @defgroup stack Stack (Stack)
 * @brief Last-In, First-Out container implementation
 * @{
 */

/**
 * @brief Opaque type definition for the stack structure
 */
typedef struct cobalt_stack cobalt_stack_t;

/**
 * @brief Create a new stack
 * @return Returns the stack pointer on success, or NULL if memory allocation fails.
 */
cobalt_stack_t *cobalt_stack_create(void);

/**
 * @brief Destroy the stack and free memory
 * @param stack Pointer to the stack to be destroyed
 * @note Will free all stack nodes, but will not free the memory pointed to by the data pointers (void *item) stored in the stack.
 */
void cobalt_stack_destroy(cobalt_stack_t *stack);

/**
 * @brief Push an element onto the top of the stack
 * @param stack Pointer to the stack
 * @param item Pointer to the element to push
 * @return Returns 0 on success, -1 on failure (e.g., memory allocation failed or stack is NULL).
 */
int cobalt_stack_push(cobalt_stack_t *stack, void *item);

/**
 * @brief Pop an element from the top of the stack
 * @param stack Pointer to the stack
 * @return The element pointer popped from the top of the stack. Returns NULL if the stack is empty or stack is NULL.
 */
void *cobalt_stack_pop(cobalt_stack_t *stack);

/**
 * @brief Peek at the top element of the stack without popping it
 * @param stack Pointer to the stack
 * @return The element pointer at the top of the stack. Returns NULL if the stack is empty or stack is NULL.
 */
void *cobalt_stack_peek(cobalt_stack_t *stack);

/**
 * @brief Get the number of elements in the stack
 * @param stack Pointer to the stack
 * @return The number of elements in the stack. Returns 0 if stack is NULL.
 */
size_t cobalt_stack_size(cobalt_stack_t *stack);

/**
 * @brief Check if the stack is empty
 * @param stack Pointer to the stack
 * @return Returns 1 if the stack's size is 0, returns 0 if it is not empty or stack is NULL.
 */
int cobalt_stack_is_empty(cobalt_stack_t *stack);

/** @} */

#endif /* STACK_H */
