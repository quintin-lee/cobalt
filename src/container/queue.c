#include "cobalt/container/queue.h"
#include <stdlib.h>

typedef struct queue_node
{
    void* data;
    struct queue_node* next;
} queue_node_t;

struct cobalt_queue
{
    queue_node_t* head;
    queue_node_t* tail;
    size_t size;
};

cobalt_queue_t* cobalt_queue_create(void)
{
    cobalt_queue_t* queue = malloc(sizeof(cobalt_queue_t));
    if (!queue)
        return NULL;
    queue->head = queue->tail = NULL;
    queue->size = 0;
    return queue;
}

void cobalt_queue_destroy(cobalt_queue_t* queue)
{
    if (!queue)
        return;

    queue_node_t* node = queue->head;
    while (node)
        {
            queue_node_t* next = node->next;
            free(node);
            node = next;
        }
    free(queue);
}

int cobalt_queue_enqueue(cobalt_queue_t* queue, void* item)
{
    if (!queue)
        return -1;

    queue_node_t* node = malloc(sizeof(queue_node_t));
    if (!node)
        return -1;

    node->data = item;
    node->next = NULL;

    if (!queue->tail)
        {
            queue->head = queue->tail = node;
        }
    else
        {
            queue->tail->next = node;
            queue->tail = node;
        }
    queue->size++;
    return 0;
}

void* cobalt_queue_dequeue(cobalt_queue_t* queue)
{
    if (!queue || !queue->head)
        return NULL;

    queue_node_t* node = queue->head;
    void* data = node->data;
    queue->head = node->next;
    if (!queue->head)
        queue->tail = NULL;
    queue->size--;
    free(node);
    return data;
}

void* cobalt_queue_peek(cobalt_queue_t* queue)
{
    if (!queue || !queue->head)
        return NULL;
    return queue->head->data;
}

size_t cobalt_queue_size(cobalt_queue_t* queue)
{
    return queue ? queue->size : 0;
}

int cobalt_queue_is_empty(cobalt_queue_t* queue)
{
    return queue && queue->size == 0;
}
