/**
 * @file error_stack.c
 * @brief Error stack implementation
 */

#include "cobalt/runtime/error_stack.h"

/**
 * @brief Capture the current thread-local error onto the stack
 *
 * @param stack Error stack to push onto
 * @return COBALT_SUCCESS on success, COBALT_ERROR_INVALID_ARGUMENT for NULL
 *         stack, COBALT_ERROR_OUT_OF_MEMORY when the stack is full
 */
cobalt_error_t cobalt_error_stack_push(cobalt_error_stack_t *stack)
{
    if (!stack) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    if (stack->depth >= COBALT_ERROR_STACK_MAX_DEPTH) {
        return COBALT_ERROR_OUT_OF_MEMORY;
    }
    stack->entries[stack->depth] = cobalt_error_get_current();
    stack->depth++;
    return COBALT_SUCCESS;
}

/**
 * @brief Restore the most recently captured error as current
 *
 * @param stack Error stack to pop from; no-op on NULL or empty stack
 */
void cobalt_error_stack_pop(cobalt_error_stack_t *stack)
{
    if (!stack || stack->depth == 0) {
        return;
    }
    cobalt_error_set(NULL, stack->entries[--stack->depth]);
}

/**
 * @brief Discard all captured errors and reset current to success
 *
 * @param stack Error stack to clear; no-op on NULL stack
 */
void cobalt_error_stack_clear(cobalt_error_stack_t *stack)
{
    if (!stack) {
        return;
    }
    stack->depth = 0;
    cobalt_error_set(NULL, COBALT_SUCCESS);
}

/**
 * @brief Query the number of captured errors
 *
 * @param stack Error stack to inspect
 * @return Current depth, or 0 for NULL stack
 */
size_t cobalt_error_stack_depth(const cobalt_error_stack_t *stack)
{
    return stack ? stack->depth : 0;
}

/**
 * @brief Check whether the stack reached maximum depth
 *
 * @param stack Error stack to inspect
 * @return Non-zero when full (or stack is NULL), 0 otherwise
 */
int cobalt_error_stack_is_full(const cobalt_error_stack_t *stack)
{
    return stack ? (stack->depth >= COBALT_ERROR_STACK_MAX_DEPTH) : 1;
}
