#include "cobalt/container/deque.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

typedef struct deque_node {
    void              *data;
    struct deque_node *next;
    struct deque_node *prev;
} deque_node_t;

struct cobalt_deque {
    deque_node_t *head;
    deque_node_t *tail;
    size_t        size;
};

cobalt_deque_t *cobalt_deque_create(void)
{
    cobalt_deque_t *deque = malloc(sizeof(cobalt_deque_t));
    if (!deque) {
        return NULL;
    }
    deque->head = NULL;
    deque->tail = NULL;
    deque->size = 0;
    return deque;
}

void cobalt_deque_destroy(cobalt_deque_t *deque)
{
    if (!deque) {
        return;
    }

    deque_node_t *node = deque->head;
    while (node) {
        deque_node_t *next = node->next;
        free(node);
        node = next;
    }
    free(deque);
}

int cobalt_deque_push_front(cobalt_deque_t *deque, void *item)
{
    if (!deque) {
        return -1;
    }

    deque_node_t *node = malloc(sizeof(deque_node_t));
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

int cobalt_deque_push_back(cobalt_deque_t *deque, void *item)
{
    if (!deque) {
        return -1;
    }

    deque_node_t *node = malloc(sizeof(deque_node_t));
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

void *cobalt_deque_pop_front(cobalt_deque_t *deque)
{
    if (!deque || !deque->head) {
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

    free(node);
    deque->size--;
    return data;
}

void *cobalt_deque_pop_back(cobalt_deque_t *deque)
{
    if (!deque || !deque->tail) {
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

    free(node);
    deque->size--;
    return data;
}

void *cobalt_deque_peek_front(cobalt_deque_t *deque)
{
    if (!deque || !deque->head) {
        return NULL;
    }
    return deque->head->data;
}

void *cobalt_deque_peek_back(cobalt_deque_t *deque)
{
    if (!deque || !deque->tail) {
        return NULL;
    }
    return deque->tail->data;
}

size_t cobalt_deque_size(cobalt_deque_t *deque)
{
    return deque ? deque->size : 0;
}

int cobalt_deque_is_empty(cobalt_deque_t *deque)
{
    return deque ? deque->size == 0 : 1;
}
