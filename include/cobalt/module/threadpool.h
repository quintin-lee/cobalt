#ifndef COBALT_MODULE_THREADPOOL_H
#define COBALT_MODULE_THREADPOOL_H

/**
 * @file threadpool.h
 * @brief High-performance work-stealing thread pool and task scheduler
 * @details Implements a hybrid task scheduler combining a global lock-free MPMC queue
 *          with per-worker Chase-Lev SPMC work-stealing deques. Provides asynchronous task
 *          dispatch, WaitGroup synchronization barriers with help-first work-stealing,
 *          and dynamic chunk-partitioned parallel loops.
 *
 * @defgroup ThreadPool Thread Pool
 * @ingroup Modules
 * @{
 */

#include "cobalt/memory/allocator.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Asynchronous task callback function signature
 * @param arg User-supplied argument
 */
typedef void (*cobalt_task_fn_t)(void *arg);

/**
 * @brief Parallel-for loop callback function signature
 * @param index Current iteration index
 * @param arg User-supplied argument
 */
typedef void (*cobalt_parallel_for_fn_t)(size_t index, void *arg);

/**
 * @brief Advanced thread pool configuration parameters
 */
typedef struct {
    size_t num_threads;        /**< Worker count (0 = auto-detect CPU cores) */
    size_t local_queue_size;   /**< Per-worker deque capacity (default 1024, power of 2) */
    size_t global_queue_size;  /**< Global MPMC queue capacity (default 65536, power of 2) */
    cobalt_allocator_t *alloc; /**< Memory allocator (NULL = system allocator) */
} cobalt_threadpool_config_t;

/**
 * @brief Opaque thread pool handle
 */
typedef struct cobalt_threadpool cobalt_threadpool_t;

/**
 * @brief Opaque WaitGroup synchronization barrier handle
 */
typedef struct cobalt_waitgroup cobalt_waitgroup_t;

/* -------------------------------------------------------------------------- */
/* Thread Pool Lifecycle                                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create a thread pool with the specified number of worker threads
 * @param num_threads Number of worker threads (0 = auto-detect CPU cores)
 * @return Pointer to created thread pool, or NULL on failure
 */
cobalt_threadpool_t *cobalt_threadpool_create(size_t num_threads);

/**
 * @brief Create a thread pool with custom configuration parameters
 * @param config Pointer to configuration struct (must not be NULL)
 * @return Pointer to created thread pool, or NULL on failure
 */
cobalt_threadpool_t *cobalt_threadpool_create_with_config(const cobalt_threadpool_config_t *config);

/**
 * @brief Shut down the thread pool, wait for all queued tasks to finish, and release resources
 * @param pool Thread pool to destroy. No-op if NULL.
 */
void cobalt_threadpool_destroy(cobalt_threadpool_t *pool);

/* -------------------------------------------------------------------------- */
/* Task Submission & Parallel Execution                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Submit an asynchronous task to the thread pool
 * @details If called from within a thread pool worker thread, the task is pushed directly
 *          to the worker's local Chase-Lev deque with zero dynamic allocations. If called from
 *          an external thread, it is enqueued into the global MPMC queue.
 * @param pool Target thread pool
 * @param fn Task function to execute
 * @param arg Opaque argument passed to the task function
 * @return 0 on success, -1 on failure or if pool is shutting down
 */
int cobalt_threadpool_submit(cobalt_threadpool_t *pool, cobalt_task_fn_t fn, void *arg);

/**
 * @brief Execute a loop in parallel across thread pool workers
 * @details Automatically partitions the range [start, end) into balanced chunks. The calling
 *          thread participates in chunk execution, minimizing submission latency. Blocks until
 *          all iterations have completed.
 * @param pool Target thread pool
 * @param start Starting index (inclusive)
 * @param end Ending index (exclusive)
 * @param chunk_size Granularity of work per task (0 = auto-calculate optimal size)
 * @param fn Function invoked for each iteration index
 * @param arg Opaque argument passed to fn
 * @return 0 on success, -1 on invalid arguments or pool failure
 */
int cobalt_threadpool_parallel_for(cobalt_threadpool_t     *pool,
                                   size_t                   start,
                                   size_t                   end,
                                   size_t                   chunk_size,
                                   cobalt_parallel_for_fn_t fn,
                                   void                    *arg);

/**
 * @brief Block the caller until all queued and active tasks in the pool have finished
 * @param pool Target thread pool
 */
void cobalt_threadpool_wait_idle(cobalt_threadpool_t *pool);

/* -------------------------------------------------------------------------- */
/* Introspection                                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Get the number of worker threads in the pool
 * @param pool Target thread pool
 * @return Number of worker threads, or 0 if pool is NULL
 */
size_t cobalt_threadpool_thread_count(const cobalt_threadpool_t *pool);

/**
 * @brief Get the number of currently active or queued tasks
 * @param pool Target thread pool
 * @return Active task count
 */
size_t cobalt_threadpool_active_tasks(const cobalt_threadpool_t *pool);

/* -------------------------------------------------------------------------- */
/* WaitGroup Synchronization Barrier                                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create and initialize a new WaitGroup
 * @return Pointer to created WaitGroup, or NULL on allocation failure
 */
cobalt_waitgroup_t *cobalt_waitgroup_create(void);

/**
 * @brief Add a delta to the WaitGroup counter
 * @param wg Target WaitGroup
 * @param delta Number of tasks to add (may be positive or negative)
 */
void cobalt_waitgroup_add(cobalt_waitgroup_t *wg, int64_t delta);

/**
 * @brief Decrement the WaitGroup counter by 1
 * @param wg Target WaitGroup
 */
void cobalt_waitgroup_done(cobalt_waitgroup_t *wg);

/**
 * @brief Block until the WaitGroup counter reaches 0
 * @details If called from a thread pool worker thread, the worker executes "help-first
 * work-stealing" (actively popping local tasks and stealing from peers) rather than sleeping,
 * completely eliminating thread starvation deadlocks.
 * @param wg Target WaitGroup
 */
void cobalt_waitgroup_wait(cobalt_waitgroup_t *wg);

/**
 * @brief Destroy a WaitGroup and free its resources
 * @param wg WaitGroup to destroy. No-op if NULL.
 */
void cobalt_waitgroup_destroy(cobalt_waitgroup_t *wg);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* COBALT_MODULE_THREADPOOL_H */
