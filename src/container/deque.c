/**
 * @file deque.c
 * @brief Implementation file of the double-ended queue (Deque) container
 *
 * Implements the double-ended queue interfaces defined in deque.h. Internally implemented using a
 * doubly-linked list, supporting insertion and deletion operations with O(1) time complexity at
 * both the head and tail.
 */

#include "cobalt/container/deque.h"
#include "cobalt/interface/iterator.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

/**
 * @brief Internal doubly-linked list node structure
 */
typedef struct deque_node {
    void              *data; /**< Pointer to stored data */
    struct deque_node *next; /**< Pointer to the next node */
    struct deque_node *prev; /**< Pointer to the previous node */
} deque_node_t;

/**
 * @brief Internal structure implementation of the double-ended queue
 */
struct cobalt_deque {
    cobalt_sequence_t base; /**< Base sequence interface (for polymorphism) */
    deque_node_t     *head; /**< Queue head node pointer */
    deque_node_t     *tail; /**< Queue tail node pointer */
    size_t            size; /**< Current number of elements in the queue */
};

/* ========================================================================= */
/* Sequence Interface vtable                                                 */
/* ========================================================================= */

static size_t deque_size_seq(cobalt_sequence_t *self)
{
    cobalt_deque_t *deque = (cobalt_deque_t *)self;
    return deque->size;
}

static int deque_is_empty_seq(cobalt_sequence_t *self)
{
    cobalt_deque_t *deque = (cobalt_deque_t *)self;
    return deque->size == 0;
}

static void deque_add_seq(cobalt_sequence_t *self, void *item)
{
    cobalt_deque_t *deque = (cobalt_deque_t *)self;
    cobalt_deque_push_back(deque, item);
}

static void deque_remove_seq(cobalt_sequence_t *self, void *item)
{
    cobalt_deque_t *deque = (cobalt_deque_t *)self;
    if (!deque || !item) {
        return;
    }
    deque_node_t *node = deque->head;
    while (node) {
        if (node->data == item) {
            if (node->prev) {
                node->prev->next = node->next;
            } else {
                deque->head = node->next;
            }
            if (node->next) {
                node->next->prev = node->prev;
            } else {
                deque->tail = node->prev;
            }
            free(node);
            deque->size--;
            return;
        }
        node = node->next;
    }
}

static void *deque_get_at_index_seq(cobalt_sequence_t *self, size_t index)
{
    cobalt_deque_t *deque = (cobalt_deque_t *)self;
    if (!deque || index >= deque->size) {
        return NULL;
    }
    /* Traverse from the closer end */
    if (index * 2 < deque->size) {
        deque_node_t *node = deque->head;
        for (size_t i = 0; i < index; i++) {
            node = node->next;
        }
        return node->data;
    } else {
        deque_node_t *node = deque->tail;
        for (size_t i = deque->size - 1; i > index; i--) {
            node = node->prev;
        }
        return node->data;
    }
}

static cobalt_iterator_t *deque_iterator_seq(cobalt_sequence_t *self)
{
    return cobalt_iterator_new(self);
}

/* ========================================================================= */
/**
 * @brief Create a new double-ended queue
 */
cobalt_deque_t *cobalt_deque_create(void)
{
    cobalt_deque_t *deque = malloc(sizeof(cobalt_deque_t));
    if (!deque) {
        return NULL;
    }
    deque->base.size         = deque_size_seq;
    deque->base.is_empty     = deque_is_empty_seq;
    deque->base.add          = deque_add_seq;
    deque->base.remove       = deque_remove_seq;
    deque->base.get_at_index = deque_get_at_index_seq;
    deque->base.iterator     = deque_iterator_seq;
    deque->head              = NULL;
    deque->tail              = NULL;
    deque->size              = 0;
    return deque;
}

/**
 * @brief Destroy a double-ended queue, free all nodes and its own memory
 */
void cobalt_deque_destroy(cobalt_deque_t *deque)
{
    if (!deque) {
        return;
    }

    deque_node_t *node = deque->head;
    // Iterate through the doubly-linked list, free the memory of each node
    while (node) {
        deque_node_t *next = node->next;
        free(node);
        node = next;
    }
    // Free the queue structure itself
    free(deque);
}

/**
 * @brief Insert an element at the front of the double-ended queue
 */
int cobalt_deque_push_front(cobalt_deque_t *deque, void *item)
{
    if (!deque) {
        return -1;
    }

    // Create a new node
    deque_node_t *node = malloc(sizeof(deque_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    node->data = item;
    node->prev = NULL;
    node->next = deque->head; // New node points to current head node

    // If the original queue is not empty, update the previous pointer of the original head node
    if (deque->head) {
        deque->head->prev = node;
    } else {
        // If the original queue is empty, the tail node also points to the new node
        deque->tail = node;
    }

    deque->head = node; // Update head node to new node
    deque->size++;
    return 0;
}

/**
 * @brief Insert an element at the back of the double-ended queue
 */
int cobalt_deque_push_back(cobalt_deque_t *deque, void *item)
{
    if (!deque) {
        return -1;
    }

    // Create a new node
    deque_node_t *node = malloc(sizeof(deque_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    node->data = item;
    node->next = NULL;
    node->prev = deque->tail; // Previous pointer of new node points to current tail node

    // If the original queue is not empty, update the next pointer of the original tail node
    if (deque->tail) {
        deque->tail->next = node;
    } else {
        // If the original queue is empty, the head node also points to the new node
        deque->head = node;
    }

    deque->tail = node; // Update tail node to new node
    deque->size++;
    return 0;
}

/**
 * @brief Remove and return the element at the front of the double-ended queue
 */
void *cobalt_deque_pop_front(cobalt_deque_t *deque)
{
    if (!deque || !deque->head) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }

    deque_node_t *node = deque->head;
    void         *data = node->data;

    // Move head pointer backward
    deque->head = node->next;
    if (deque->head) {
        deque->head->prev = NULL; // Nullify previous pointer of the new head node
    } else {
        // If the queue is empty after removal, the tail pointer must also be nullified
        deque->tail = NULL;
    }

    free(node); // Free the memory of the original head node
    deque->size--;
    return data;
}

/**
 * @brief Remove and return the element at the back of the double-ended queue
 */
void *cobalt_deque_pop_back(cobalt_deque_t *deque)
{
    if (!deque || !deque->tail) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }

    deque_node_t *node = deque->tail;
    void         *data = node->data;

    // Move tail pointer forward
    deque->tail = node->prev;
    if (deque->tail) {
        deque->tail->next = NULL; // Nullify next pointer of the new tail node
    } else {
        // If the queue is empty after removal, the head pointer must also be nullified
        deque->head = NULL;
    }

    free(node); // Free the memory of the original tail node
    deque->size--;
    return data;
}

/**
 * @brief Get the element at the front of the double-ended queue (without removing it)
 */
void *cobalt_deque_peek_front(cobalt_deque_t *deque)
{
    if (!deque || !deque->head) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }
    return deque->head->data;
}

/**
 * @brief Get the element at the back of the double-ended queue (without removing it)
 */
void *cobalt_deque_peek_back(cobalt_deque_t *deque)
{
    if (!deque || !deque->tail) {
        cobalt_error_set(NULL, COBALT_ERROR_EMPTY_CONTAINER);
        return NULL;
    }
    return deque->tail->data;
}

/**
 * @brief Get the number of elements in the double-ended queue
 */
size_t cobalt_deque_size(const cobalt_deque_t *deque)
{
    return deque ? deque->size : 0;
}

/**
 * @brief Check if the double-ended queue is empty
 */
int cobalt_deque_is_empty(const cobalt_deque_t *deque)
{
    // If deque is NULL, it is also treated as an empty queue and returns 1
    return deque ? deque->size == 0 : 1;
}
