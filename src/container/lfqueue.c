/**
 * @file lfqueue.c
 * @brief Lock-free bounded Multi-Producer Multi-Consumer (MPMC) queue
 *
 * @details Implements Dmitry Vyukov's bounded MPMC queue with C11 atomics and
 *          explicit 64-byte cache-line padding to avoid false sharing.
 */

#include "cobalt/container/lfqueue.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/runtime/error.h"
#include <stdalign.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define COBALT_CACHE_LINE_SIZE 64

/**
 * @brief Cell in the circular buffer
 */
typedef struct {
    atomic_size_t sequence; /**< Monotonic sequence indicating cell occupancy */
    void         *data;     /**< User payload pointer */
} cobalt_lfqueue_cell_t;

/**
 * @brief Internal lock-free queue control structure
 */
struct cobalt_lfqueue {
    /* Producer hot zone */
    _Alignas(COBALT_CACHE_LINE_SIZE) atomic_size_t tail;

    /* Consumer hot zone */
    _Alignas(COBALT_CACHE_LINE_SIZE) atomic_size_t head;

    /* Configuration and buffer zone */
    _Alignas(COBALT_CACHE_LINE_SIZE) size_t capacity;
    size_t                 mask;
    cobalt_allocator_t    *alloc;
    cobalt_lfqueue_cell_t *buffer;
};

static inline size_t lfqueue_next_pow2(size_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
#ifdef __LP64__
    v |= v >> 32;
#endif
    v++;
    return v < 2 ? 2 : v;
}

cobalt_lfqueue_t *cobalt_lfqueue_create(size_t capacity)
{
    return cobalt_lfqueue_create_with_allocator(capacity, cobalt_allocator_get_system());
}

cobalt_lfqueue_t *cobalt_lfqueue_create_with_allocator(size_t capacity, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return NULL;
    }

    size_t cap = lfqueue_next_pow2(capacity < 2 ? 2 : capacity);

    cobalt_lfqueue_t *queue = (cobalt_lfqueue_t *)alloc->alloc(alloc, sizeof(cobalt_lfqueue_t));
    if (!queue) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    queue->buffer =
        (cobalt_lfqueue_cell_t *)alloc->alloc(alloc, sizeof(cobalt_lfqueue_cell_t) * cap);
    if (!queue->buffer) {
        alloc->free(alloc, queue);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    for (size_t i = 0; i < cap; i++) {
        atomic_init(&queue->buffer[i].sequence, i);
        queue->buffer[i].data = NULL;
    }

    atomic_init(&queue->tail, 0);
    atomic_init(&queue->head, 0);
    queue->capacity = cap;
    queue->mask     = cap - 1;
    queue->alloc    = alloc;

    return queue;
}

void cobalt_lfqueue_destroy(cobalt_lfqueue_t *queue)
{
    if (!queue) {
        return;
    }
    queue->alloc->free(queue->alloc, queue->buffer);
    queue->alloc->free(queue->alloc, queue);
}

int cobalt_lfqueue_enqueue(cobalt_lfqueue_t *queue, void *data)
{
    if (!queue) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }

    cobalt_lfqueue_cell_t *cell;
    size_t                 pos = atomic_load_explicit(&queue->tail, memory_order_relaxed);

    for (;;) {
        cell          = &queue->buffer[pos & queue->mask];
        size_t   seq  = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &queue->tail, &pos, pos + 1, memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            /* Queue is full */
            return -1;
        } else {
            pos = atomic_load_explicit(&queue->tail, memory_order_relaxed);
        }
    }

    cell->data = data;
    atomic_store_explicit(&cell->sequence, pos + 1, memory_order_release);
    return 0;
}

int cobalt_lfqueue_dequeue(cobalt_lfqueue_t *queue, void **out_data)
{
    if (!queue || !out_data) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }

    cobalt_lfqueue_cell_t *cell;
    size_t                 pos = atomic_load_explicit(&queue->head, memory_order_relaxed);

    for (;;) {
        cell          = &queue->buffer[pos & queue->mask];
        size_t   seq  = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &queue->head, &pos, pos + 1, memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            /* Queue is empty */
            return -1;
        } else {
            pos = atomic_load_explicit(&queue->head, memory_order_relaxed);
        }
    }

    *out_data = cell->data;
    atomic_store_explicit(&cell->sequence, pos + queue->mask + 1, memory_order_release);
    return 0;
}

size_t cobalt_lfqueue_size(const cobalt_lfqueue_t *queue)
{
    if (!queue) {
        return 0;
    }
    size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    if (tail >= head) {
        return tail - head;
    }
    return 0;
}

int cobalt_lfqueue_is_empty(const cobalt_lfqueue_t *queue)
{
    if (!queue) {
        return 1;
    }
    size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    return head >= tail;
}

int cobalt_lfqueue_is_full(const cobalt_lfqueue_t *queue)
{
    if (!queue) {
        return 0;
    }
    size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    return (tail - head) >= queue->capacity;
}

size_t cobalt_lfqueue_capacity(const cobalt_lfqueue_t *queue)
{
    return queue ? queue->capacity : 0;
}
