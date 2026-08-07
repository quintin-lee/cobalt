/**
 * @file error_stack_demo.c
 * @brief Demonstrates error stack usage for nested error handling
 *
 * Shows how to save and restore error state across function calls
 * using the cobalt error stack.
 */

#include <cobalt/cobalt.h>
#include <stdio.h>

/* Simulated deep call chain that may fail */
static cobalt_error_t process_level_3(void)
{
    cobalt_error_set(NULL, COBALT_ERROR_IO);
    return COBALT_ERROR_IO;
}

static cobalt_error_t process_level_2(void)
{
    cobalt_error_stack_t stack;
    cobalt_error_stack_clear(&stack);

    cobalt_error_stack_push(&stack);
    cobalt_error_t result = process_level_3();
    cobalt_error_stack_pop(&stack);

    return result;
}

static cobalt_error_t process_level_1(void)
{
    cobalt_error_stack_t stack;
    cobalt_error_stack_clear(&stack);

    /* Save current error state */
    cobalt_error_stack_push(&stack);

    /* Simulate some work */
    cobalt_error_t err = COBALT_SUCCESS;
    cobalt_error_set(&err, COBALT_ERROR_TIMEOUT);
    printf("  Level 1: set timeout error: %s\n", cobalt_error_get_message(err));

    /* Call deeper processing */
    cobalt_error_t deep_err = process_level_2();
    printf("  Level 1: deep process returned: %s\n", cobalt_error_get_message(deep_err));

    /* Restore original error state */
    cobalt_error_stack_pop(&stack);

    /* Error state is restored */
    cobalt_error_t current = cobalt_error_get_current();
    printf("  Level 1: after restore, current error: %s\n", cobalt_error_get_message(current));

    return COBALT_SUCCESS;
}

int main(void)
{
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    printf("=== Error Stack Demo ===\n\n");

    cobalt_info("Initial error state: %s\n", cobalt_error_get_message(cobalt_error_get_current()));

    cobalt_error_t result = process_level_1();
    printf("\nprocess_level_1 returned: %s\n", cobalt_error_get_message(result));

    /* Demonstrate nested error stacks */
    printf("\n--- Nested stacks ---\n");

    cobalt_error_stack_t outer_stack;
    cobalt_error_stack_clear(&outer_stack);

    cobalt_error_stack_push(&outer_stack);
    cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
    printf("  Outer: set OOM error\n");

    cobalt_error_stack_push(&outer_stack);
    cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
    printf("  Inner: set NOT_FOUND error\n");
    printf("  Inner depth: %zu\n", cobalt_error_stack_depth(&outer_stack));

    cobalt_error_stack_pop(&outer_stack);
    printf("  After pop, current error: %s\n",
           cobalt_error_get_message(cobalt_error_get_current()));
    printf("  Depth after pop: %zu\n", cobalt_error_stack_depth(&outer_stack));

    cobalt_error_stack_pop(&outer_stack);
    printf("  After second pop, current error: %s\n",
           cobalt_error_get_message(cobalt_error_get_current()));
    printf("  Depth after second pop: %zu\n", cobalt_error_stack_depth(&outer_stack));

    printf("\n=== Demo complete ===\n");
    return 0;
}
