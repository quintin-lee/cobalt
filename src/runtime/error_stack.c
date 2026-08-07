/**
 * @file error_stack.c
 * @brief Error stack implementation
 */

#include "cobalt/runtime/error_stack.h"

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

void cobalt_error_stack_pop(cobalt_error_stack_t *stack)
{
    if (!stack || stack->depth == 0) {
        return;
    }
    cobalt_error_set(NULL, stack->entries[--stack->depth]);
}

void cobalt_error_stack_clear(cobalt_error_stack_t *stack)
{
    if (!stack) {
        return;
    }
    stack->depth = 0;
    cobalt_error_set(NULL, COBALT_SUCCESS);
}

size_t cobalt_error_stack_depth(const cobalt_error_stack_t *stack)
{
    return stack ? stack->depth : 0;
}

int cobalt_error_stack_is_full(const cobalt_error_stack_t *stack)
{
    return stack ? (stack->depth >= COBALT_ERROR_STACK_MAX_DEPTH) : 1;
}
