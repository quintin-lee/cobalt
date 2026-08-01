#ifndef QUEUE_H
#define QUEUE_H

/**
 * @file queue.h
 * @brief Queue (FIFO) container
 */

#include <stddef.h>

typedef struct cobalt_queue cobalt_queue_t;

/* Create a new queue */
cobalt_queue_t *cobalt_queue_create(void);

/* Destroy the queue */
void cobalt_queue_destroy(cobalt_queue_t *queue);

/* Enqueue item */
int cobalt_queue_enqueue(cobalt_queue_t *queue, void *item);

/* Dequeue item */
void *cobalt_queue_dequeue(cobalt_queue_t *queue);

/* Peek at front item */
void *cobalt_queue_peek(cobalt_queue_t *queue);

/* Get queue size */
size_t cobalt_queue_size(cobalt_queue_t *queue);

/* Check if queue is empty */
int cobalt_queue_is_empty(cobalt_queue_t *queue);

#endif /* QUEUE_H */
