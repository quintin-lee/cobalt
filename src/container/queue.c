/**
 * @file queue.c
 * @brief Implementation file of the Queue (FIFO) container
 *
 * Implements the queue interfaces defined in queue.h. Internally implemented using a singly-linked
 * list, supporting enqueue (add to tail) and dequeue (remove from head) operations with O(1) time
 * complexity.
 */

#include "cobalt/container/queue.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

/**
 * @brief Internal singly-linked list node structure
 */
typedef struct queue_node {
    void              *data; /**< Pointer to stored data */
    struct queue_node *next; /**< Pointer to the next node */
} queue_node_t;

/**
 * @brief Internal structure implementation of the queue
 */
struct cobalt_queue {
    queue_node_t *head; /**< Queue head node pointer, used for dequeue */
    queue_node_t *tail; /**< Queue tail node pointer, used for enqueue */
    size_t        size; /**< Current number of elements in the queue */
};

/**
 * @brief Create a new queue
 */
cobalt_queue_t *cobalt_queue_create(void)
{
    cobalt_queue_t *queue = malloc(sizeof(cobalt_queue_t));
    if (!queue) {
        return NULL;
    }
    queue->head = queue->tail = NULL;
    queue->size               = 0;
    return queue;
}

/**
 * @brief Destroy a queue, free all nodes and its own memory
 */
void cobalt_queue_destroy(cobalt_queue_t *queue)
{
    if (!queue) {
        return;
    }

    queue_node_t *node = queue->head;
    // Iterate through the singly-linked list, free the memory of each node
    while (node) {
        queue_node_t *next = node->next;
        free(node);
        node = next;
    }
    // Free the queue structure itself
    free(queue);
}

/**
 * @brief Add an element to the tail of the queue (enqueue)
 */
int cobalt_queue_enqueue(cobalt_queue_t *queue, void *item)
{
    if (!queue) {
        return -1;
    }

    // Create a new node
    queue_node_t *node = malloc(sizeof(queue_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    node->data = item;
    node->next = NULL;

    // If the queue was originally empty, both head and tail point to the new node
    if (!queue->tail) {
        queue->head = queue->tail = node;
    } else {
        // Modify the next pointer of the original tail node, and update the tail node
        queue->tail->next = node;
        queue->tail       = node;
    }
    queue->size++;
    return 0;
}

/**
 * @brief Remove and return the element at the head of the queue (dequeue)
 */
void *cobalt_queue_dequeue(cobalt_queue_t *queue)
{
    if (!queue || !queue->head) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }

    queue_node_t *node = queue->head;
    void         *data = node->data;

    // Move head pointer backward
    queue->head = node->next;
    // If the queue is empty after removal, the tail pointer must also be nullified
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;
    free(node); // Free the memory of the original head node
    return data;
}

/**
 * @brief Get the element at the head of the queue (without removing it)
 */
void *cobalt_queue_peek(cobalt_queue_t *queue)
{
    if (!queue || !queue->head) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }
    return queue->head->data;
}

/**
 * @brief Get the number of elements in the queue
 */
size_t cobalt_queue_size(cobalt_queue_t *queue)
{
    return queue ? queue->size : 0;
}

/**
 * @brief Check if the queue is empty
 */
int cobalt_queue_is_empty(cobalt_queue_t *queue)
{
    return queue && queue->size == 0;
}
