/**
 * @file threadpool.c
 * @brief High-performance work-stealing thread pool and task scheduler
 */

#include "cobalt/module/threadpool.h"
#include "cobalt/container/lfqueue.h"
#include "cobalt/platform/atomic.h"
#include "cobalt/platform/platform.h"
#include "cobalt/platform/thread.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define COBALT_THREADPOOL_DEFAULT_LOCAL_CAPACITY 1024
#define COBALT_THREADPOOL_DEFAULT_GLOBAL_CAPACITY 4096

/* -------------------------------------------------------------------------- */
/* Internal Types & Chase-Lev SPMC Deque                                      */
/* -------------------------------------------------------------------------- */

typedef struct {
    cobalt_task_fn_t fn;
    void            *arg;
} cobalt_task_t;

typedef struct {
    cobalt_task_t *buffer;
    size_t         capacity;
    size_t         mask;
    _Alignas(64) atomic_size_t top;
    _Alignas(64) atomic_size_t bottom;
} chase_lev_deque_t;

static int chase_lev_init(chase_lev_deque_t *q, size_t capacity, cobalt_allocator_t *alloc)
{
    if (capacity < 2) {
        capacity = 2;
    }
    /* Round up to nearest power of 2 */
    size_t cap = 1;
    while (cap < capacity) {
        cap <<= 1;
    }

    q->capacity = cap;
    q->mask     = cap - 1;
    atomic_init(&q->top, 0);
    atomic_init(&q->bottom, 0);

    size_t buf_bytes = cap * sizeof(cobalt_task_t);
    q->buffer        = alloc ? (cobalt_task_t *)alloc->alloc(alloc, buf_bytes)
                             : (cobalt_task_t *)malloc(buf_bytes);
    return q->buffer ? 0 : -1;
}

static void chase_lev_destroy(chase_lev_deque_t *q, cobalt_allocator_t *alloc)
{
    if (q->buffer) {
        if (alloc) {
            alloc->free(alloc, q->buffer);
        } else {
            free(q->buffer);
        }
        q->buffer = NULL;
    }
}

static int chase_lev_push(chase_lev_deque_t *q, cobalt_task_t task)
{
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
    if (b - t >= q->capacity) {
        return -1; /* Deque is full */
    }
    q->buffer[b & q->mask] = task;
    atomic_store_explicit(&q->bottom, b + 1, memory_order_release);
    return 0;
}

static int chase_lev_pop(chase_lev_deque_t *q, cobalt_task_t *out_task)
{
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    if (b == 0) {
        return -1;
    }
    b--;
    atomic_store_explicit(&q->bottom, b, memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);

    size_t t = atomic_load_explicit(&q->top, memory_order_relaxed);
    if (t <= b) {
        *out_task = q->buffer[b & q->mask];
        if (t == b) {
            /* Last remaining element: compete with potential concurrent stealers */
            size_t expected_t = t;
            if (!atomic_compare_exchange_strong_explicit(
                    &q->top, &expected_t, t + 1, memory_order_seq_cst, memory_order_relaxed)) {
                /* Stealer won the race */
                atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
                return -1;
            }
            atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
        }
        return 0;
    } else {
        /* Deque was empty */
        atomic_store_explicit(&q->bottom, t, memory_order_relaxed);
        return -1;
    }
}

static int chase_lev_steal(chase_lev_deque_t *q, cobalt_task_t *out_task)
{
    size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    size_t b = atomic_load_explicit(&q->bottom, memory_order_acquire);

    if (t < b) {
        *out_task = q->buffer[t & q->mask];
        if (atomic_compare_exchange_weak_explicit(
                &q->top, &t, t + 1, memory_order_seq_cst, memory_order_relaxed)) {
            return 0;
        }
    }
    return -1;
}

static size_t chase_lev_size(const chase_lev_deque_t *q)
{
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    size_t t = atomic_load_explicit(&q->top, memory_order_relaxed);
    return (b >= t) ? (b - t) : 0;
}

/* -------------------------------------------------------------------------- */
/* Worker and Thread Pool Declarations                                        */
/* -------------------------------------------------------------------------- */

typedef struct cobalt_worker {
    cobalt_thread_t           thread_handle;
    size_t                    worker_id;
    struct cobalt_threadpool *pool;
    chase_lev_deque_t         local_deque;
    _Alignas(64) atomic_uint_fast64_t tasks_executed;
} cobalt_worker_t;

struct cobalt_threadpool {
    size_t              num_workers;
    cobalt_worker_t    *workers;
    cobalt_lfqueue_t   *global_queue;
    cobalt_allocator_t *alloc;
    atomic_bool         running;
    atomic_size_t       active_tasks;
    atomic_size_t       idle_workers;
    cobalt_mutex_t     *idle_mutex;
    cobalt_cond_t      *idle_cond;
    cobalt_mutex_t     *drain_mutex;
    cobalt_cond_t      *drain_cond;
};

__attribute__((unused)) static _Thread_local cobalt_worker_t *tl_current_worker = NULL;

/* -------------------------------------------------------------------------- */
/* Temporary stubs for Task 1 linkage                                         */
/* -------------------------------------------------------------------------- */

cobalt_threadpool_t *cobalt_threadpool_create(size_t num_threads)
{
    cobalt_threadpool_config_t config = {
        .num_threads       = num_threads,
        .local_queue_size  = COBALT_THREADPOOL_DEFAULT_LOCAL_CAPACITY,
        .global_queue_size = COBALT_THREADPOOL_DEFAULT_GLOBAL_CAPACITY,
        .alloc             = NULL,
    };
    return cobalt_threadpool_create_with_config(&config);
}

cobalt_threadpool_t *cobalt_threadpool_create_with_config(const cobalt_threadpool_config_t *config)
{
    (void)config;
    return NULL;
}

void cobalt_threadpool_destroy(cobalt_threadpool_t *pool)
{
    (void)pool;
}

int cobalt_threadpool_submit(cobalt_threadpool_t *pool, cobalt_task_fn_t fn, void *arg)
{
    (void)pool;
    (void)fn;
    (void)arg;
    return -1;
}

int cobalt_threadpool_parallel_for(cobalt_threadpool_t     *pool,
                                   size_t                   start,
                                   size_t                   end,
                                   size_t                   chunk_size,
                                   cobalt_parallel_for_fn_t fn,
                                   void                    *arg)
{
    (void)pool;
    (void)start;
    (void)end;
    (void)chunk_size;
    (void)fn;
    (void)arg;
    return -1;
}

void cobalt_threadpool_wait_idle(cobalt_threadpool_t *pool)
{
    (void)pool;
}

size_t cobalt_threadpool_thread_count(const cobalt_threadpool_t *pool)
{
    return pool ? pool->num_workers : 0;
}

size_t cobalt_threadpool_active_tasks(const cobalt_threadpool_t *pool)
{
    return pool ? atomic_load(&pool->active_tasks) : 0;
}

/* WaitGroup structures and stubs */
struct cobalt_waitgroup {
    atomic_int_fast64_t counter;
    cobalt_mutex_t     *mutex;
    cobalt_cond_t      *cond;
};

cobalt_waitgroup_t *cobalt_waitgroup_create(void)
{
    return NULL;
}

void cobalt_waitgroup_add(cobalt_waitgroup_t *wg, int64_t delta)
{
    (void)wg;
    (void)delta;
}

void cobalt_waitgroup_done(cobalt_waitgroup_t *wg)
{
    (void)wg;
}

void cobalt_waitgroup_wait(cobalt_waitgroup_t *wg)
{
    (void)wg;
}

void cobalt_waitgroup_destroy(cobalt_waitgroup_t *wg)
{
    (void)wg;
}

/* -------------------------------------------------------------------------- */
/* Test-accessible Helpers for Chase-Lev Deque Verification                  */
/* -------------------------------------------------------------------------- */

int cobalt_test_chase_lev_init(void **deque_out, size_t capacity)
{
    chase_lev_deque_t *q = (chase_lev_deque_t *)malloc(sizeof(chase_lev_deque_t));
    if (!q) {
        return -1;
    }
    if (chase_lev_init(q, capacity, NULL) != 0) {
        free(q);
        return -1;
    }
    *deque_out = q;
    return 0;
}

void cobalt_test_chase_lev_destroy(void *deque_ptr)
{
    if (deque_ptr) {
        chase_lev_deque_t *q = (chase_lev_deque_t *)deque_ptr;
        chase_lev_destroy(q, NULL);
        free(q);
    }
}

int cobalt_test_chase_lev_push(void *deque_ptr, void (*fn)(void *), void *arg)
{
    chase_lev_deque_t *q    = (chase_lev_deque_t *)deque_ptr;
    cobalt_task_t      task = {.fn = fn, .arg = arg};
    return chase_lev_push(q, task);
}

int cobalt_test_chase_lev_pop(void *deque_ptr, void (**out_fn)(void *), void **out_arg)
{
    chase_lev_deque_t *q = (chase_lev_deque_t *)deque_ptr;
    cobalt_task_t      task;
    if (chase_lev_pop(q, &task) == 0) {
        if (out_fn) {
            *out_fn = task.fn;
        }
        if (out_arg) {
            *out_arg = task.arg;
        }
        return 0;
    }
    return -1;
}

int cobalt_test_chase_lev_steal(void *deque_ptr, void (**out_fn)(void *), void **out_arg)
{
    chase_lev_deque_t *q = (chase_lev_deque_t *)deque_ptr;
    cobalt_task_t      task;
    if (chase_lev_steal(q, &task) == 0) {
        if (out_fn) {
            *out_fn = task.fn;
        }
        if (out_arg) {
            *out_arg = task.arg;
        }
        return 0;
    }
    return -1;
}

size_t cobalt_test_chase_lev_size(void *deque_ptr)
{
    return deque_ptr ? chase_lev_size((const chase_lev_deque_t *)deque_ptr) : 0;
}
