/**
 * @file test_stack.c
 * @Unit test for stack container.
 */

#include "cobalt/container/stack.h"
#include "test_framework.h"
#include <stdio.h>

void test_stack_basic(void)
{
    printf("Testing stack basic operations...\n");

    cobalt_stack_t* stack = cobalt_stack_create();
    if (!stack)
        {
            fprintf(stderr, "ERROR: Failed to create stack\n");
            return;
        }

    /* Test empty */
    if (cobalt_stack_is_empty(stack))
        {
            printf("  Stack is empty: OK\n");
        }

    /* Push items */
    int a = 1, b = 2, c = 3;
    cobalt_stack_push(stack, &a);
    cobalt_stack_push(stack, &b);
    cobalt_stack_push(stack, &c);

    if (cobalt_stack_size(stack) == 3)
        {
            printf("  Stack size after 3 pushes: 3 OK\n");
        }

    /* Peek */
    int* top = (int*)cobalt_stack_peek(stack);
    if (top && *top == 3)
        {
            printf("  Peek returns 3: OK\n");
        }

    /* Pop - LIFO order */
    int* val = (int*)cobalt_stack_pop(stack);
    if (val && *val == 3)
        {
            printf("  Pop returns 3: OK\n");
        }

    val = (int*)cobalt_stack_pop(stack);
    if (val && *val == 2)
        {
            printf("  Pop returns 2: OK\n");
        }

    val = (int*)cobalt_stack_pop(stack);
    if (val && *val == 1)
        {
            printf("  Pop returns 1: OK\n");
        }

    if (cobalt_stack_is_empty(stack))
        {
            printf("  Stack is empty after pops: OK\n");
        }

    cobalt_stack_destroy(stack);
    printf("  Stack tests completed\n");
}

void test_stack_edge_cases(void)
{
    printf("Testing stack edge cases...\n");

    /* Pop from empty stack */
    cobalt_stack_t* stack = cobalt_stack_create();
    TEST_ASSERT(stack != NULL);
    TEST_ASSERT(cobalt_stack_peek(stack) == NULL);
    TEST_ASSERT(cobalt_stack_pop(stack) == NULL);
    TEST_ASSERT(cobalt_stack_is_empty(stack));
    TEST_ASSERT(cobalt_stack_size(stack) == 0);

    /* Push to NULL stack */
    TEST_ASSERT(cobalt_stack_push(NULL, NULL) == -1);

    /* Destroy NULL stack */
    cobalt_stack_destroy(NULL); /* should not crash */

    cobalt_stack_destroy(stack);
    printf("  Stack edge cases OK\n");
}

void test_stack(void)
{
    printf("Testing stack...\n");
    test_stack_basic();
    test_stack_edge_cases();
}
