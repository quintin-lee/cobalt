#ifndef COBALT_RUNTIME_ERROR_STACK_H
#define COBALT_RUNTIME_ERROR_STACK_H

/**
 * @file error_stack.h
 * @brief Error stack — push/pop error state for deep call chains
 * @details Provides a thread-local error stack that allows saving the current error state,
 *          performing operations that may fail, and restoring the previous state on scope exit.
 *
 * Usage:
 *   cobalt_error_stack_t stack;
 *   cobalt_error_stack_clear(&stack);
 *   cobalt_error_stack_push(&stack);
 *   // ... operations that may set errors ...
 *   cobalt_error_stack_pop(&stack);
 *
 * @defgroup RuntimeErrorStack Error stack
 * @ingroup RuntimeError
 * @{
 */

#include "cobalt/runtime/error.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Maximum depth of the error stack per thread
 */
enum { COBALT_ERROR_STACK_MAX_DEPTH = 32 };

/**
 * @brief Error stack type — stack-allocatable
 */
typedef struct cobalt_error_stack {
    /** Error entries stored in the stack */
    cobalt_error_t entries[COBALT_ERROR_STACK_MAX_DEPTH];
    /** Current number of entries in the stack */
    size_t depth;
} cobalt_error_stack_t;

/**
 * @brief Save current error state onto the stack
 * @param stack Stack to push onto
 * @return COBALT_SUCCESS on success, COBALT_ERROR_OUT_OF_MEMORY if stack is full
 */
cobalt_error_t cobalt_error_stack_push(cobalt_error_stack_t *stack);

/**
 * @brief Restore the error state saved by the most recent push
 * @param stack Stack to pop from
 */
void cobalt_error_stack_pop(cobalt_error_stack_t *stack);

/**
 * @brief Clear all frames from the stack (resets to COBALT_SUCCESS)
 * @param stack Stack to clear
 */
void cobalt_error_stack_clear(cobalt_error_stack_t *stack);

/**
 * @brief Get the number of frames currently on the stack
 * @param stack Stack to query
 * @return Number of frames
 */
size_t cobalt_error_stack_depth(const cobalt_error_stack_t *stack);

/**
 * @brief Check if the stack is full
 * @param stack Stack to check
 * @return 1 if full, 0 otherwise
 */
int cobalt_error_stack_is_full(const cobalt_error_stack_t *stack);

/** @} */

#endif /* COBALT_RUNTIME_ERROR_STACK_H */
