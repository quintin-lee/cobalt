/**
 * @file test_error_stack.c
 * @brief Unit test for error stack.
 */

#include "cobalt/runtime/error_stack.h"
#include "test_framework.h"
#include <stdio.h>

void test_error_stack_basic(void)
{
    printf("Testing error stack basic push/pop...\n");

    cobalt_error_stack_t stack;
    cobalt_error_stack_clear(&stack);

    cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_ERROR_OUT_OF_MEMORY);

    TEST_ASSERT(cobalt_error_stack_push(&stack) == COBALT_SUCCESS);
    TEST_ASSERT(cobalt_error_stack_depth(&stack) == 1);

    cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_ERROR_INVALID_ARGUMENT);

    cobalt_error_stack_pop(&stack);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_ERROR_OUT_OF_MEMORY);
    TEST_ASSERT(cobalt_error_stack_depth(&stack) == 0);

    printf("  Basic push/pop: OK\n");
}

void test_error_stack_nested(void)
{
    printf("Testing nested push/pop...\n");

    cobalt_error_stack_t stack;
    cobalt_error_stack_clear(&stack);

    cobalt_error_set(NULL, COBALT_SUCCESS);
    cobalt_error_stack_push(&stack); /* frame 0: SUCCESS */

    cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
    cobalt_error_stack_push(&stack); /* frame 1: NOT_FOUND */

    cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
    cobalt_error_stack_push(&stack); /* frame 2: OOM */

    TEST_ASSERT(cobalt_error_get_current() == COBALT_ERROR_OUT_OF_MEMORY);
    cobalt_error_stack_pop(&stack);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_ERROR_OUT_OF_MEMORY);
    cobalt_error_stack_pop(&stack);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_ERROR_NOT_FOUND);
    cobalt_error_stack_pop(&stack);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_SUCCESS);

    printf("  Nested push/pop: OK\n");
}

void test_error_stack_overflow(void)
{
    printf("Testing stack overflow protection...\n");

    cobalt_error_stack_t stack;
    cobalt_error_stack_clear(&stack);

    for (size_t i = 0; i < COBALT_ERROR_STACK_MAX_DEPTH; i++) {
        TEST_ASSERT(cobalt_error_stack_push(&stack) == COBALT_SUCCESS);
    }
    TEST_ASSERT(cobalt_error_stack_depth(&stack) == COBALT_ERROR_STACK_MAX_DEPTH);
    TEST_ASSERT(cobalt_error_stack_is_full(&stack) == 1);

    /* Overflow should return error */
    TEST_ASSERT(cobalt_error_stack_push(&stack) != COBALT_SUCCESS);

    /* Pop all frames */
    for (size_t i = 0; i < COBALT_ERROR_STACK_MAX_DEPTH; i++) {
        cobalt_error_stack_pop(&stack);
    }
    TEST_ASSERT(cobalt_error_stack_depth(&stack) == 0);

    printf("  Overflow protection: OK\n");
}

void test_error_stack_null_safe(void)
{
    printf("Testing null safety...\n");

    cobalt_error_stack_clear(NULL);
    cobalt_error_stack_pop(NULL);
    TEST_ASSERT(cobalt_error_stack_depth(NULL) == 0);
    TEST_ASSERT(cobalt_error_stack_is_full(NULL) == 1);
    printf("  Null safety: OK\n");
}

void test_error_stack_clear(void)
{
    printf("Testing stack clear...\n");

    cobalt_error_stack_t stack;
    cobalt_error_stack_clear(&stack);

    cobalt_error_set(NULL, COBALT_ERROR_TIMEOUT);
    cobalt_error_stack_push(&stack);
    cobalt_error_set(NULL, COBALT_ERROR_IO);
    cobalt_error_stack_push(&stack);

    cobalt_error_stack_clear(&stack);
    TEST_ASSERT(cobalt_error_get_current() == COBALT_SUCCESS);
    TEST_ASSERT(cobalt_error_stack_depth(&stack) == 0);

    printf("  Clear: OK\n");
}

void test_error_stack(void)
{
    printf("Testing error stack...\n");
    test_error_stack_basic();
    test_error_stack_nested();
    test_error_stack_overflow();
    test_error_stack_null_safe();
    test_error_stack_clear();
    printf("  Error stack tests completed\n");
}
