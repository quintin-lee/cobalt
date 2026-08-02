#include "cobalt/container/stack.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

typedef struct stack_node {
    void              *data;
    struct stack_node *next;
} stack_node_t;

struct cobalt_stack {
    stack_node_t *top;
    size_t        size;
};

cobalt_stack_t *cobalt_stack_create(void)
{
    cobalt_stack_t *stack = malloc(sizeof(cobalt_stack_t));
    if (!stack) {
        return NULL;
    }
    stack->top  = NULL;
    stack->size = 0;
    return stack;
}

void cobalt_stack_destroy(cobalt_stack_t *stack)
{
    if (!stack) {
        return;
    }

    stack_node_t *node = stack->top;
    while (node) {
        stack_node_t *next = node->next;
        free(node);
        node = next;
    }
    free(stack);
}

int cobalt_stack_push(cobalt_stack_t *stack, void *item)
{
    if (!stack) {
        return -1;
    }

    stack_node_t *node = malloc(sizeof(stack_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    node->data = item;
    node->next = stack->top;
    stack->top = node;
    stack->size++;
    return 0;
}

void *cobalt_stack_pop(cobalt_stack_t *stack)
{
    if (!stack || !stack->top) {
        return NULL;
    }

    stack_node_t *node = stack->top;
    void         *data = node->data;
    stack->top         = node->next;
    stack->size--;
    free(node);
    return data;
}

void *cobalt_stack_peek(cobalt_stack_t *stack)
{
    if (!stack || !stack->top) {
        return NULL;
    }
    return stack->top->data;
}

size_t cobalt_stack_size(cobalt_stack_t *stack)
{
    return stack ? stack->size : 0;
}

int cobalt_stack_is_empty(cobalt_stack_t *stack)
{
    return stack && stack->size == 0;
}
