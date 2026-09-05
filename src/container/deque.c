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
    cobalt_sequence_t   base; /**< Base sequence interface (for polymorphism) */
    cobalt_allocator_t *alloc;
    deque_node_t       *head; /**< Queue head node pointer */
    deque_node_t       *tail; /**< Queue tail node pointer */
    size_t              size; /**< Current number of elements in the queue */
};

/* ========================================================================= */
/* Sequence Interface vtable                                                 */
/* ========================================================================= */

/**
 * @brief report element count through sequence interface slot
 * @param self sequence interface pointer
 * @return number of elements
 */
static size_t deque_size_seq(cobalt_sequence_t *self)
{
    cobalt_deque_t *deque = (cobalt_deque_t *)self;
    return deque->size;
}

/**
 * @brief report emptiness through sequence interface slot
 * @param self sequence interface pointer
 * @return nonzero when deque holds no elements
 */
static int deque_is_empty_seq(cobalt_sequence_t *self)
{
    cobalt_deque_t *deque = (cobalt_deque_t *)self;
    return deque->size == 0;
}

/**
 * @brief append element through sequence interface slot
 * @param self sequence interface pointer
 * @param item element to append at back
 */
static void deque_add_seq(cobalt_sequence_t *self, void *item)
{
    cobalt_deque_t *deque = (cobalt_deque_t *)self;
    cobalt_deque_push_back(deque, item);
}

/**
 * @brief remove first matching element through sequence interface slot
 * @param self sequence interface pointer
 * @param item element pointer to remove by identity
 */
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
            deque->alloc->free(deque->alloc, node);
            deque->size--;
            return;
        }
        node = node->next;
    }
}

/**
 * @brief fetch element by index through sequence interface slot
 * @param self sequence interface pointer
 * @param index position to fetch
 * @return element pointer, or NULL when index out of range
 * @note traverses from closer end for shorter walk
 */
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
    }
    deque_node_t *node = deque->tail;
    for (size_t i = deque->size - 1; i > index; i--) {
        node = node->prev;
    }
    return node->data;
}

/**
 * @brief create iterator through sequence interface slot
 * @param self sequence interface pointer
 * @return new iterator over deque elements
 */
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
    return cobalt_deque_create_with_allocator(cobalt_allocator_get_system());
}

cobalt_deque_t *cobalt_deque_create_with_allocator(cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_deque_t *deque = (cobalt_deque_t *)alloc->alloc(alloc, sizeof(cobalt_deque_t));
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
    deque->alloc             = alloc;
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
    while (node) {
        deque_node_t *next = node->next;
        deque->alloc->free(deque->alloc, node);
        node = next;
    }
    deque->alloc->free(deque->alloc, deque);
}

/**
 * @brief Insert an element at the front of the double-ended queue
 */
int cobalt_deque_push_front(cobalt_deque_t *deque, void *item)
{
    if (!deque) {
        return -1;
    }

    deque_node_t *node = (deque_node_t *)deque->alloc->alloc(deque->alloc, sizeof(deque_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    node->data = item;
    node->prev = NULL;
    node->next = deque->head;

    if (deque->head) {
        deque->head->prev = node;
    } else {
        deque->tail = node;
    }
    deque->head = node;
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

    deque_node_t *node = (deque_node_t *)deque->alloc->alloc(deque->alloc, sizeof(deque_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    node->data = item;
    node->next = NULL;
    node->prev = deque->tail;

    if (deque->tail) {
        deque->tail->next = node;
    } else {
        deque->head = node;
    }
    deque->tail = node;
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

    deque->head = node->next;
    if (deque->head) {
        deque->head->prev = NULL;
    } else {
        deque->tail = NULL;
    }
    deque->alloc->free(deque->alloc, node);
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

    deque->tail = node->prev;
    if (deque->tail) {
        deque->tail->next = NULL;
    } else {
        deque->head = NULL;
    }
    deque->alloc->free(deque->alloc, node);
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
    return deque ? deque->size == 0 : 1;
}
