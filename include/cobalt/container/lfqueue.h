#ifndef COBALT_CONTAINER_LFQUEUE_H
#define COBALT_CONTAINER_LFQUEUE_H

/**
 * @file lfqueue.h
 * @brief Lock-free bounded Multi-Producer Multi-Consumer (MPMC) queue
 * @details Implements Dmitry Vyukov's bounded MPMC queue using C11 atomics (`stdatomic.h`).
 *          Features 64-byte cache-line aligned producer and consumer cursors to eliminate
 *          false sharing across CPU cores, providing ultra-high concurrent throughput.
 *
 * @defgroup lfqueue Lock-Free Queue (LFQueue)
 * @ingroup Containers
 * @{
 */

#include "cobalt/memory/allocator.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque structure representing the bounded lock-free queue
 */
typedef struct cobalt_lfqueue cobalt_lfqueue_t;

/**
 * @brief Create a new lock-free queue with the system allocator
 * @param capacity Requested capacity. Will be rounded up to the nearest power of two (min 2).
 * @return Pointer to the created queue, or NULL on allocation failure
 */
cobalt_lfqueue_t *cobalt_lfqueue_create(size_t capacity);

/**
 * @brief Create a new lock-free queue with a custom allocator
 * @param capacity Requested capacity. Will be rounded up to the nearest power of two (min 2).
 * @param alloc Custom allocator instance (must not be NULL)
 * @return Pointer to the created queue, or NULL on failure
 */
cobalt_lfqueue_t *cobalt_lfqueue_create_with_allocator(size_t capacity, cobalt_allocator_t *alloc);

/**
 * @brief Destroy a lock-free queue and free all its internal memory
 * @param queue Pointer to the queue to destroy
 * @note Does not free user payload pointers stored in the queue.
 */
void cobalt_lfqueue_destroy(cobalt_lfqueue_t *queue);

/**
 * @brief Enqueue an item into the queue (non-blocking)
 * @param queue Pointer to the lock-free queue
 * @param data Opaque data pointer to enqueue
 * @return 0 on success, -1 if queue is full or invalid arguments
 */
int cobalt_lfqueue_enqueue(cobalt_lfqueue_t *queue, void *data);

/**
 * @brief Dequeue an item from the queue (non-blocking)
 * @param queue Pointer to the lock-free queue
 * @param out_data Pointer to location where dequeued data pointer will be stored
 * @return 0 on success, -1 if queue is empty or invalid arguments
 */
int cobalt_lfqueue_dequeue(cobalt_lfqueue_t *queue, void **out_data);

/**
 * @brief Get the approximate current number of elements in the queue
 * @param queue Pointer to the queue
 * @return Number of elements, or 0 if queue is NULL
 */
size_t cobalt_lfqueue_size(const cobalt_lfqueue_t *queue);

/**
 * @brief Check whether the queue is empty
 * @param queue Pointer to the queue
 * @return Non-zero if empty, 0 otherwise
 */
int cobalt_lfqueue_is_empty(const cobalt_lfqueue_t *queue);

/**
 * @brief Check whether the queue is full
 * @param queue Pointer to the queue
 * @return Non-zero if full, 0 otherwise
 */
int cobalt_lfqueue_is_full(const cobalt_lfqueue_t *queue);

/**
 * @brief Get the total capacity of the queue
 * @param queue Pointer to the queue
 * @return Capacity (always a power of two), or 0 if queue is NULL
 */
size_t cobalt_lfqueue_capacity(const cobalt_lfqueue_t *queue);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* COBALT_CONTAINER_LFQUEUE_H */
