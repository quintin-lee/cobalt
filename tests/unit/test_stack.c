/**
 * @file test_stack.c
 * @Unit test for stack container.
 */

#include <stdio.h>
#include "cobalt/container/stack.h"

void test_stack_basic(void) {
    printf("Testing stack basic operations...\n");
    
    cobalt_stack_t *stack = cobalt_stack_create();
    if (!stack) {
        fprintf(stderr, "ERROR: Failed to create stack\n");
        return;
    }
    
    /* Test empty */
    if (cobalt_stack_is_empty(stack)) {
        printf("  Stack is empty: OK\n");
    }
    
    /* Push items */
    int a = 1, b = 2, c = 3;
    cobalt_stack_push(stack, &a);
    cobalt_stack_push(stack, &b);
    cobalt_stack_push(stack, &c);
    
    if (cobalt_stack_size(stack) == 3) {
        printf("  Stack size after 3 pushes: 3 OK\n");
    }
    
    /* Peek */
    int *top = (int *)cobalt_stack_peek(stack);
    if (top && *top == 3) {
        printf("  Peek returns 3: OK\n");
    }
    
    /* Pop - LIFO order */
    int *val = (int *)cobalt_stack_pop(stack);
    if (val && *val == 3) {
        printf("  Pop returns 3: OK\n");
    }
    
    val = (int *)cobalt_stack_pop(stack);
    if (val && *val == 2) {
        printf("  Pop returns 2: OK\n");
    }
    
    val = (int *)cobalt_stack_pop(stack);
    if (val && *val == 1) {
        printf("  Pop returns 1: OK\n");
    }
    
    if (cobalt_stack_is_empty(stack)) {
        printf("  Stack is empty after pops: OK\n");
    }
    
    cobalt_stack_destroy(stack);
    printf("  Stack tests completed\n");
}

void test_stack(void) {
    printf("Testing stack...\n");
    test_stack_basic();
}
