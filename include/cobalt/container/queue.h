#ifndef QUEUE_H
#define QUEUE_H

/**
 * @file queue.h
 * @brief Queue (FIFO) container
 *
 * A queue is a First-In-First-Out (FIFO) linear data structure,
 * suitable for scenarios such as task scheduling and message buffering.
 */

#include <stddef.h>

/**
 * @brief Forward declaration of opaque queue type
 */
typedef struct cobalt_queue cobalt_queue_t;

/**
 * @defgroup queue_api Queue operation interfaces
 * @{
 */

/**
 * @brief Create a new queue
 *
 * @return Returns a pointer to the newly created queue on success; returns NULL if memory
 * allocation fails.
 */
cobalt_queue_t *cobalt_queue_create(void);

/**
 * @brief Destroy a queue and free its memory
 *
 * Frees the memory of all nodes in the queue as well as the memory of the queue container itself.
 * @param queue Pointer to the queue to be destroyed
 * @warning This operation does not free the memory of the user data itself stored in the queue
 * nodes.
 */
void cobalt_queue_destroy(cobalt_queue_t *queue);

/**
 * @brief Add an element to the tail of the queue (enqueue)
 *
 * @param queue Pointer to the target queue
 * @param item Pointer to the user data to be added
 * @return Returns 0 on success; returns -1 if queue is NULL or memory allocation fails.
 */
int cobalt_queue_enqueue(cobalt_queue_t *queue, void *item);

/**
 * @brief Remove and return the element at the head of the queue (dequeue)
 *
 * @param queue Pointer to the target queue
 * @return User data pointer stored in the original head node; returns NULL if the queue is empty or
 * queue is NULL.
 */
void *cobalt_queue_dequeue(cobalt_queue_t *queue);

/**
 * @brief Get the element at the head of the queue (without removing it)
 *
 * @param queue Pointer to the target queue
 * @return User data pointer stored in the head node; returns NULL if the queue is empty or queue is
 * NULL.
 */
void *cobalt_queue_peek(cobalt_queue_t *queue);

/**
 * @brief Get the number of elements in the queue
 *
 * @param queue Pointer to the target queue
 * @return The number of elements in the queue; returns 0 if queue is NULL.
 */
size_t cobalt_queue_size(const cobalt_queue_t *queue);

/**
 * @brief Check if the queue is empty
 *
 * @param queue Pointer to the target queue
 * @return Returns 1 if the queue is not NULL and the number of elements is 0; otherwise returns 0.
 * If queue is NULL, returns 0.
 */
int cobalt_queue_is_empty(const cobalt_queue_t *queue);

/** @} */

#endif /* QUEUE_H */
