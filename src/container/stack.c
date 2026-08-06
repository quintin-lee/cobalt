/**
 * @file stack.c
 * @brief Stack container implementation
 * @details Last-In, First-Out (LIFO) stack structure implemented based on a singly linked list.
 * Each push operation inserts a new node at the head of the linked list.
 */

#include "cobalt/container/stack.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

/**
 * @brief Stack node structure, forming a singly linked list
 */
typedef struct stack_node {
    void *data; /* Data pointer stored */
    struct stack_node
        *next; /* Pointer to the next node, i.e., the element further down in the stack */
} stack_node_t;

/**
 * @brief Internal representation structure of the stack
 */
struct cobalt_stack {
    stack_node_t *top;  /* Pointer to the top node of the stack */
    size_t        size; /* Total number of elements in the stack */
};

/**
 * @brief Create and initialize an empty stack
 *
 * @return Returns the newly created stack pointer, or NULL if out of memory
 */
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

/**
 * @brief Destroy the stack, freeing memory for all nodes
 * @note Does not free the user data (data) memory, if needed, it should be manually popped and
 * freed before destruction
 *
 * @param stack Pointer to the stack
 */
void cobalt_stack_destroy(cobalt_stack_t *stack)
{
    if (!stack) {
        return;
    }

    stack_node_t *node = stack->top;
    // Iterate and free every node in the linked list
    while (node) {
        stack_node_t *next = node->next;
        free(node);
        node = next;
    }
    free(stack);
}

/**
 * @brief Push an element onto the top of the stack
 * @details Creates a new node and uses head insertion to link it to the top of the stack
 *
 * @param stack Pointer to the stack
 * @param item Data pointer to push
 * @return Returns 0 on success, -1 on failure and sets COBALT_ERROR_OUT_OF_MEMORY error
 */
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
    node->next = stack->top; // New node points to original top of the stack
    stack->top = node;       // Update top of stack to new node
    stack->size++;
    return 0;
}

/**
 * @brief Pop the top element of the stack
 * @details Removes and returns the data of the top node, then frees the node's memory
 *
 * @param stack Pointer to the stack
 * @return Returns the data pointer of the top element, or NULL if the stack is empty
 */
void *cobalt_stack_pop(cobalt_stack_t *stack)
{
    if (!stack || !stack->top) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }

    stack_node_t *node = stack->top;
    void         *data = node->data;
    stack->top         = node->next; // Move stack top down
    stack->size--;
    free(node); // Free original top node
    return data;
}

/**
 * @brief Peek but do not remove the top element of the stack
 *
 * @param stack Pointer to the stack
 * @return Returns the data pointer of the top element, or NULL if the stack is empty
 */
void *cobalt_stack_peek(cobalt_stack_t *stack)
{
    if (!stack || !stack->top) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }
    return stack->top->data;
}

/**
 * @brief Get the current size of the stack
 *
 * @param stack Pointer to the stack
 * @return The number of elements in the stack
 */
size_t cobalt_stack_size(const cobalt_stack_t *stack)
{
    return stack ? stack->size : 0;
}

/**
 * @brief Check if the stack is empty
 *
 * @param stack Pointer to the stack
 * @return Returns 1 if the stack's size is 0, otherwise (or when stack is NULL) returns 0
 */
int cobalt_stack_is_empty(const cobalt_stack_t *stack)
{
    return stack && stack->size == 0;
}
