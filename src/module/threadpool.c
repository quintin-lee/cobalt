/**
 * @file threadpool.c
 * @brief High-performance work-stealing thread pool and task scheduler
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "cobalt/module/threadpool.h"
#include "cobalt/container/lfqueue.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/platform/atomic.h"
#include "cobalt/platform/platform.h"
#include "cobalt/platform/thread.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#define COBALT_THREADPOOL_DEFAULT_LOCAL_CAPACITY 1024
#define COBALT_THREADPOOL_DEFAULT_GLOBAL_CAPACITY 65536

/* -------------------------------------------------------------------------- */
/* CPU Core Auto-Detection                                                    */
/* -------------------------------------------------------------------------- */

static size_t detect_cpu_cores(void)
{
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors > 0 ? (size_t)si.dwNumberOfProcessors : 4;
#elif defined(_SC_NPROCESSORS_ONLN)
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    return cores > 0 ? (size_t)cores : 4;
#else
    return 4;
#endif
}

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

static _Thread_local cobalt_worker_t *tl_current_worker = NULL;

/* -------------------------------------------------------------------------- */
/* Worker Thread Routine & Task Execution Pipeline                            */
/* -------------------------------------------------------------------------- */

static bool worker_find_and_execute_task(cobalt_worker_t *worker)
{
    cobalt_threadpool_t *pool = worker->pool;
    cobalt_task_t        task;
    bool                 found = false;

    /* 1. Local Chase-Lev deque (LIFO) */
    if (chase_lev_pop(&worker->local_deque, &task) == 0) {
        found = true;
    }

    /* 2. Global MPMC queue */
    if (!found) {
        cobalt_task_t *global_task_ptr = NULL;
        if (cobalt_lfqueue_dequeue(pool->global_queue, (void **)&global_task_ptr) == 0 &&
            global_task_ptr) {
            task = *global_task_ptr;
            pool->alloc->free(pool->alloc, global_task_ptr);
            found = true;
        }
    }

    /* 3. Work-Stealing from peers (FIFO) */
    if (!found && pool->num_workers > 1) {
        size_t victim_idx = (worker->worker_id + 1) % pool->num_workers;
        for (size_t i = 0; i < pool->num_workers - 1; i++) {
            if (victim_idx != worker->worker_id) {
                if (chase_lev_steal(&pool->workers[victim_idx].local_deque, &task) == 0) {
                    found = true;
                    break;
                }
            }
            victim_idx = (victim_idx + 1) % pool->num_workers;
        }
    }

    if (found) {
        if (task.fn) {
            task.fn(task.arg);
        }
        atomic_fetch_add_explicit(&worker->tasks_executed, 1, memory_order_relaxed);
        size_t prev = atomic_fetch_sub_explicit(&pool->active_tasks, 1, memory_order_acq_rel);
        if (prev == 1) {
            /* Active tasks drained to zero */
            cobalt_mutex_lock(pool->drain_mutex);
            cobalt_cond_broadcast(pool->drain_cond);
            cobalt_mutex_unlock(pool->drain_mutex);
        }
        return true;
    }

    return false;
}

static void *worker_thread_func(void *arg)
{
    cobalt_worker_t     *worker = (cobalt_worker_t *)arg;
    cobalt_threadpool_t *pool   = worker->pool;
    tl_current_worker           = worker;

    while (atomic_load_explicit(&pool->running, memory_order_relaxed) ||
           atomic_load_explicit(&pool->active_tasks, memory_order_relaxed) > 0) {
        if (worker_find_and_execute_task(worker)) {
            continue;
        }

        /* Check termination condition before sleeping */
        if (!atomic_load_explicit(&pool->running, memory_order_relaxed) &&
            atomic_load_explicit(&pool->active_tasks, memory_order_relaxed) == 0) {
            break;
        }

        /* Adaptive spin-yield before blocking */
        bool arrived = false;
        for (int spin = 0; spin < 32; spin++) {
            if (chase_lev_size(&worker->local_deque) > 0 ||
                !cobalt_lfqueue_is_empty(pool->global_queue)) {
                arrived = true;
                break;
            }
            cobalt_thread_yield();
        }

        if (arrived) {
            continue;
        }

        /* Enter condition wait */
        atomic_fetch_add_explicit(&pool->idle_workers, 1, memory_order_release);
        cobalt_mutex_lock(pool->idle_mutex);
        if (atomic_load_explicit(&pool->running, memory_order_relaxed) &&
            chase_lev_size(&worker->local_deque) == 0 &&
            cobalt_lfqueue_is_empty(pool->global_queue) &&
            atomic_load_explicit(&pool->active_tasks, memory_order_relaxed) == 0) {
            cobalt_cond_wait(pool->idle_cond, pool->idle_mutex);
        }
        cobalt_mutex_unlock(pool->idle_mutex);
        atomic_fetch_sub_explicit(&pool->idle_workers, 1, memory_order_release);
    }

    tl_current_worker = NULL;
    return NULL;
}

/* -------------------------------------------------------------------------- */
/* Thread Pool Lifecycle API                                                  */
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
    if (!config) {
        return NULL;
    }

    size_t num_workers = config->num_threads;
    if (num_workers == 0) {
        num_workers = detect_cpu_cores();
    }
    if (num_workers == 0) {
        num_workers = 1;
    }

    cobalt_allocator_t  *alloc = config->alloc ? config->alloc : cobalt_allocator_get_system();
    cobalt_threadpool_t *pool  = (cobalt_threadpool_t *)alloc->alloc(alloc, sizeof(*pool));
    if (!pool) {
        return NULL;
    }
    memset(pool, 0, sizeof(*pool));

    pool->num_workers = num_workers;
    pool->alloc       = alloc;
    atomic_init(&pool->running, true);
    atomic_init(&pool->active_tasks, 0);
    atomic_init(&pool->idle_workers, 0);

    size_t global_cap  = config->global_queue_size > 0 ? config->global_queue_size
                                                       : COBALT_THREADPOOL_DEFAULT_GLOBAL_CAPACITY;
    pool->global_queue = cobalt_lfqueue_create_with_allocator(global_cap, alloc);
    if (!pool->global_queue) {
        alloc->free(alloc, pool);
        return NULL;
    }

    pool->idle_mutex  = cobalt_mutex_create();
    pool->idle_cond   = cobalt_cond_create();
    pool->drain_mutex = cobalt_mutex_create();
    pool->drain_cond  = cobalt_cond_create();
    if (!pool->idle_mutex || !pool->idle_cond || !pool->drain_mutex || !pool->drain_cond) {
        cobalt_threadpool_destroy(pool);
        return NULL;
    }

    size_t workers_bytes = sizeof(cobalt_worker_t) * num_workers;
    pool->workers        = (cobalt_worker_t *)alloc->alloc(alloc, workers_bytes);
    if (!pool->workers) {
        cobalt_threadpool_destroy(pool);
        return NULL;
    }
    memset(pool->workers, 0, workers_bytes);

    size_t local_cap = config->local_queue_size > 0 ? config->local_queue_size
                                                    : COBALT_THREADPOOL_DEFAULT_LOCAL_CAPACITY;

    for (size_t i = 0; i < num_workers; i++) {
        pool->workers[i].worker_id = i;
        pool->workers[i].pool      = pool;
        atomic_init(&pool->workers[i].tasks_executed, 0);
        if (chase_lev_init(&pool->workers[i].local_deque, local_cap, alloc) != 0) {
            cobalt_threadpool_destroy(pool);
            return NULL;
        }
    }

    for (size_t i = 0; i < num_workers; i++) {
        if (cobalt_thread_create(
                worker_thread_func, &pool->workers[i], &pool->workers[i].thread_handle) != 0) {
            /* If thread creation fails, shut down previously created threads */
            pool->num_workers = i;
            cobalt_threadpool_destroy(pool);
            return NULL;
        }
    }

    return pool;
}

void cobalt_threadpool_destroy(cobalt_threadpool_t *pool)
{
    if (!pool) {
        return;
    }

    /* Signal stop */
    atomic_store_explicit(&pool->running, false, memory_order_release);

    /* Wake all idle workers */
    if (pool->idle_mutex && pool->idle_cond) {
        cobalt_mutex_lock(pool->idle_mutex);
        cobalt_cond_broadcast(pool->idle_cond);
        cobalt_mutex_unlock(pool->idle_mutex);
    }

    /* Wait for active tasks to finish */
    if (pool->drain_mutex && pool->drain_cond) {
        cobalt_mutex_lock(pool->drain_mutex);
        while (atomic_load_explicit(&pool->active_tasks, memory_order_acquire) > 0) {
            cobalt_cond_wait(pool->drain_cond, pool->drain_mutex);
        }
        cobalt_mutex_unlock(pool->drain_mutex);
    }

    /* Join worker threads */
    if (pool->workers) {
        for (size_t i = 0; i < pool->num_workers; i++) {
            if (pool->workers[i].thread_handle) {
                cobalt_thread_join(pool->workers[i].thread_handle);
                pool->workers[i].thread_handle = NULL;
            }
            chase_lev_destroy(&pool->workers[i].local_deque, pool->alloc);
        }
        pool->alloc->free(pool->alloc, pool->workers);
        pool->workers = NULL;
    }

    /* Drain and free any residual global tasks */
    if (pool->global_queue) {
        void *residual = NULL;
        while (cobalt_lfqueue_dequeue(pool->global_queue, &residual) == 0 && residual) {
            pool->alloc->free(pool->alloc, residual);
        }
        cobalt_lfqueue_destroy(pool->global_queue);
        pool->global_queue = NULL;
    }

    if (pool->idle_mutex) {
        cobalt_mutex_destroy(pool->idle_mutex);
    }
    if (pool->idle_cond) {
        cobalt_cond_destroy(pool->idle_cond);
    }
    if (pool->drain_mutex) {
        cobalt_mutex_destroy(pool->drain_mutex);
    }
    if (pool->drain_cond) {
        cobalt_cond_destroy(pool->drain_cond);
    }

    cobalt_allocator_t *alloc = pool->alloc;
    alloc->free(alloc, pool);
}

/* -------------------------------------------------------------------------- */
/* Task Submission & Parallel Execution API                                   */
/* -------------------------------------------------------------------------- */

int cobalt_threadpool_submit(cobalt_threadpool_t *pool, cobalt_task_fn_t fn, void *arg)
{
    if (!pool || !fn || !atomic_load_explicit(&pool->running, memory_order_relaxed)) {
        return -1;
    }

    atomic_fetch_add_explicit(&pool->active_tasks, 1, memory_order_release);

    cobalt_task_t task = {.fn = fn, .arg = arg};

    /* Fast path: If current thread is a worker belonging to this pool */
    if (tl_current_worker && tl_current_worker->pool == pool) {
        if (chase_lev_push(&tl_current_worker->local_deque, task) == 0) {
            if (atomic_load_explicit(&pool->idle_workers, memory_order_acquire) > 0) {
                cobalt_mutex_lock(pool->idle_mutex);
                cobalt_cond_signal(pool->idle_cond);
                cobalt_mutex_unlock(pool->idle_mutex);
            }
            return 0;
        }
    }

    /* Fallback: allocate global task wrapper */
    cobalt_task_t *task_ptr = (cobalt_task_t *)pool->alloc->alloc(pool->alloc, sizeof(*task_ptr));
    if (!task_ptr) {
        atomic_fetch_sub_explicit(&pool->active_tasks, 1, memory_order_relaxed);
        return -1;
    }
    *task_ptr = task;

    if (cobalt_lfqueue_enqueue(pool->global_queue, task_ptr) != 0) {
        pool->alloc->free(pool->alloc, task_ptr);
        atomic_fetch_sub_explicit(&pool->active_tasks, 1, memory_order_relaxed);
        return -1;
    }

    /* Wake up an idle worker if available */
    if (atomic_load_explicit(&pool->idle_workers, memory_order_acquire) > 0) {
        cobalt_mutex_lock(pool->idle_mutex);
        cobalt_cond_signal(pool->idle_cond);
        cobalt_mutex_unlock(pool->idle_mutex);
    }

    return 0;
}

void cobalt_threadpool_wait_idle(cobalt_threadpool_t *pool)
{
    if (!pool) {
        return;
    }

    if (tl_current_worker && tl_current_worker->pool == pool) {
        /* Help-first waiting */
        while (atomic_load_explicit(&pool->active_tasks, memory_order_acquire) > 0) {
            if (!worker_find_and_execute_task(tl_current_worker)) {
                cobalt_thread_yield();
            }
        }
        return;
    }

    cobalt_mutex_lock(pool->drain_mutex);
    while (atomic_load_explicit(&pool->active_tasks, memory_order_acquire) > 0) {
        cobalt_cond_wait(pool->drain_cond, pool->drain_mutex);
    }
    cobalt_mutex_unlock(pool->drain_mutex);
}

size_t cobalt_threadpool_thread_count(const cobalt_threadpool_t *pool)
{
    return pool ? pool->num_workers : 0;
}

size_t cobalt_threadpool_active_tasks(const cobalt_threadpool_t *pool)
{
    return pool ? atomic_load_explicit(&pool->active_tasks, memory_order_relaxed) : 0;
}

/* -------------------------------------------------------------------------- */
/* WaitGroup Synchronization Barrier API                                      */
/* -------------------------------------------------------------------------- */

struct cobalt_waitgroup {
    atomic_int_fast64_t counter;
    cobalt_mutex_t     *mutex;
    cobalt_cond_t      *cond;
};

cobalt_waitgroup_t *cobalt_waitgroup_create(void)
{
    cobalt_waitgroup_t *wg = (cobalt_waitgroup_t *)malloc(sizeof(cobalt_waitgroup_t));
    if (!wg) {
        return NULL;
    }
    atomic_init(&wg->counter, 0);
    wg->mutex = cobalt_mutex_create();
    wg->cond  = cobalt_cond_create();
    if (!wg->mutex || !wg->cond) {
        cobalt_waitgroup_destroy(wg);
        return NULL;
    }
    return wg;
}

void cobalt_waitgroup_add(cobalt_waitgroup_t *wg, int64_t delta)
{
    if (!wg || delta == 0) {
        return;
    }
    atomic_fetch_add_explicit(&wg->counter, delta, memory_order_release);
}

void cobalt_waitgroup_done(cobalt_waitgroup_t *wg)
{
    if (!wg) {
        return;
    }
    int64_t prev = atomic_fetch_sub_explicit(&wg->counter, 1, memory_order_acq_rel);
    if (prev <= 1) {
        cobalt_mutex_lock(wg->mutex);
        cobalt_cond_broadcast(wg->cond);
        cobalt_mutex_unlock(wg->mutex);
    }
}

void cobalt_waitgroup_wait(cobalt_waitgroup_t *wg)
{
    if (!wg) {
        return;
    }

    if (tl_current_worker) {
        /* Help-first work-stealing: process tasks to avoid thread starvation deadlock */
        while (atomic_load_explicit(&wg->counter, memory_order_acquire) > 0) {
            if (!worker_find_and_execute_task(tl_current_worker)) {
                cobalt_thread_yield();
            }
        }
        return;
    }

    /* External thread: block on condition variable */
    cobalt_mutex_lock(wg->mutex);
    while (atomic_load_explicit(&wg->counter, memory_order_acquire) > 0) {
        cobalt_cond_wait(wg->cond, wg->mutex);
    }
    cobalt_mutex_unlock(wg->mutex);
}

void cobalt_waitgroup_destroy(cobalt_waitgroup_t *wg)
{
    if (!wg) {
        return;
    }
    if (wg->mutex) {
        cobalt_mutex_destroy(wg->mutex);
    }
    if (wg->cond) {
        cobalt_cond_destroy(wg->cond);
    }
    free(wg);
}

/* -------------------------------------------------------------------------- */
/* Parallel For Implementation                                                */
/* -------------------------------------------------------------------------- */

typedef struct {
    size_t                   start;
    size_t                   end;
    cobalt_parallel_for_fn_t fn;
    void                    *arg;
    cobalt_waitgroup_t      *wg;
} parallel_chunk_t;

static void parallel_chunk_worker(void *arg)
{
    parallel_chunk_t *chunk = (parallel_chunk_t *)arg;
    for (size_t i = chunk->start; i < chunk->end; i++) {
        chunk->fn(i, chunk->arg);
    }
    cobalt_waitgroup_done(chunk->wg);
    free(chunk);
}

int cobalt_threadpool_parallel_for(cobalt_threadpool_t     *pool,
                                   size_t                   start,
                                   size_t                   end,
                                   size_t                   chunk_size,
                                   cobalt_parallel_for_fn_t fn,
                                   void                    *arg)
{
    if (!pool || !fn) {
        return -1;
    }
    if (start >= end) {
        return 0;
    }

    size_t total = end - start;
    if (chunk_size == 0) {
        size_t n   = pool->num_workers > 0 ? pool->num_workers : 1;
        chunk_size = total / (n * 4);
        if (chunk_size == 0) {
            chunk_size = 1;
        }
    }

    cobalt_waitgroup_t *wg = cobalt_waitgroup_create();
    if (!wg) {
        return -1;
    }

    size_t curr = start;
    while (curr < end) {
        size_t next = curr + chunk_size;
        if (next > end) {
            next = end;
        }

        parallel_chunk_t *chunk = (parallel_chunk_t *)malloc(sizeof(parallel_chunk_t));
        if (!chunk) {
            /* Execute remaining synchronously */
            for (size_t i = curr; i < end; i++) {
                fn(i, arg);
            }
            break;
        }

        chunk->start = curr;
        chunk->end   = next;
        chunk->fn    = fn;
        chunk->arg   = arg;
        chunk->wg    = wg;

        cobalt_waitgroup_add(wg, 1);
        if (cobalt_threadpool_submit(pool, parallel_chunk_worker, chunk) != 0) {
            /* If submission fails, execute synchronously */
            parallel_chunk_worker(chunk);
        }

        curr = next;
    }

    cobalt_waitgroup_wait(wg);
    cobalt_waitgroup_destroy(wg);
    return 0;
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
