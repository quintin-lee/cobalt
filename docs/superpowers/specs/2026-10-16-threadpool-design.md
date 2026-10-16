# Cobalt High-Performance Work-Stealing Thread Pool Specification

- **Date:** 2026-10-16
- **Status:** Approved
- **Scope:** Layer 2 Modules (`include/cobalt/module/threadpool.h`, `src/module/threadpool.c`)

---

## 1. Overview & Goals

Cobalt currently provides platform threading primitives (`cobalt_thread_t`, `cobalt_mutex_t`, `cobalt_cond_t` in Layer 8) and a bounded lock-free Multi-Producer Multi-Consumer (MPMC) queue (`cobalt_lfqueue_t` in Layer 4). However, the framework lacks a centralized, high-performance **thread pool and asynchronous task scheduler** for parallel computation, asynchronous worker dispatch, and integration with the event loop.

### Primary Objectives
- **Hybrid Work-Stealing Scheduling Engine**: Combine an MPMC global queue (`cobalt_lfqueue_t`) for external thread dispatch with per-worker Chase-Lev Single-Producer Multi-Consumer (SPMC) deques.
- **Cache-Locality Maximization (LIFO / FIFO)**: Workers process their own tasks in LIFO order (warm L1/L2 cache), while stealing from victims in FIFO order (largest coarse-grained chunk of work).
- **Zero Heap Allocation for Internal Task Enqueue**: Inline pre-allocated task ring slots within worker deques, avoiding dynamic memory allocations on hot-path task submissions.
- **Rich Parallel Primitives**: Support asynchronous dispatch (`submit`), synchronization barriers (`cobalt_waitgroup_t`), and dynamic chunk-partitioned parallel loops (`cobalt_threadpool_parallel_for`).
- **Help-First Waiting**: When a worker waits on a WaitGroup, it actively steals and processes tasks instead of blocking, completely eliminating thread starvation deadlocks in recursive fork-join workflows.
- **Adaptive Idle Backoff**: 3-stage backoff (short spin with pause/yield, atomic idle registration, condition variable wait) preventing CPU burning while ensuring sub-microsecond wakeup latencies.
- **Graceful Lifecycle Management**: Auto-detection of CPU core count (`num_threads == 0`), complete draining of pending tasks on destroy, and clean ASan/LSan execution.

---

## 2. System Architecture

```
                                 External Calling Thread (App / EventLoop)
                                                   │
                      ┌────────────────────────────┴────────────────────────────┐
                      │                                                         │
                      ▼ (if current thread is Worker)                           ▼ (if external thread)
          Push to Worker Local Deque                                Enqueue to Global MPMC Queue
          (Chase-Lev Deque, LIFO bottom)                                  (cobalt_lfqueue_t)
                                                                                │
                      ┌─────────────────────────────────────────────────────────┘
                      │
   ┌──────────────────┴─────────────────────────────────────────────────────────┐
   │                                                                             │
   ▼                                     ▼                                       ▼
┌──────────────────────────┐   ┌──────────────────────────┐             ┌──────────────────────────┐
│     Worker Thread 0      │   │     Worker Thread 1      │             │    Worker Thread (N-1)   │
├──────────────────────────┤   ├──────────────────────────┤             ├──────────────────────────┤
│ Local Chase-Lev Deque    │   │ Local Chase-Lev Deque    │   ... ...   │ Local Chase-Lev Deque    │
│ - Bottom: LIFO Pop/Push  │   │ - Bottom: LIFO Pop/Push  │             │ - Bottom: LIFO Pop/Push  │
│ - Top:    FIFO Steal     │   │ - Top:    FIFO Steal     │             │ - Top:    FIFO Steal     │
└──────────────────────────┘   └──────────────────────────┘             └──────────────────────────┘
             ▲                              ▲                                         ▲
             └───────────── Steal ──────────┴───────────── Steal ─────────────────────┘
```

---

## 3. Detailed Component Designs

### 3.1 Chase-Lev Work-Stealing SPMC Deque

Each worker thread owns a cache-line aligned Chase-Lev deque:
```c
typedef struct {
    cobalt_task_t      *buffer;    /**< Circular array of task slots */
    size_t              mask;      /**< Capacity mask (capacity - 1, power of 2) */
    _Alignas(64) atomic_size_t top;    /**< Steal index accessed by thieves (FIFO) */
    _Alignas(64) atomic_size_t bottom; /**< Private index accessed by owner (LIFO) */
} chase_lev_deque_t;
```

#### 1. `push(task)` (Owner Thread Only)
- Owner reads `b = atomic_load_explicit(&bottom, memory_order_relaxed)`.
- Writes task directly into pre-allocated slot: `buffer[b & mask] = task`.
- Submits with `atomic_store_explicit(&bottom, b + 1, memory_order_release)`.
- **Zero CAS contention**: Incurring ~1-2 nanoseconds latency. If deque capacity is reached, tasks overflow into the global MPMC queue.

#### 2. `pop()` (Owner Thread Only - LIFO)
- Owner decrements: `b = atomic_load_explicit(&bottom, memory_order_relaxed) - 1`.
- `atomic_store_explicit(&bottom, b, memory_order_relaxed)`.
- Fence: `atomic_thread_fence(memory_order_seq_cst)`.
- Reads `t = atomic_load_explicit(&top, memory_order_relaxed)`.
- If $t < b$: Multiple items present; pop task from `buffer[b & mask]` with 0 CAS operations.
- If $t == b$: Exactly one item left; resolve race against potential thieves using `atomic_compare_exchange_strong(&top, &t, t + 1)`.
- If $t > b$: Empty deque; restore `bottom = top` and return NULL.

#### 3. `steal()` (Thief Worker Threads - FIFO)
- Thief reads `t = atomic_load_explicit(&top, memory_order_acquire)`.
- Fence: `atomic_thread_fence(memory_order_seq_cst)`.
- Reads `b = atomic_load_explicit(&bottom, memory_order_acquire)`.
- If $t < b$: Reads `task = buffer[t & mask]`. Attempts CAS:
  `atomic_compare_exchange_weak_explicit(&top, &t, t + 1, memory_order_seq_cst, memory_order_relaxed)`.
- On success, task is acquired; on failure, thief yields or proceeds to next victim.

---

### 3.2 Scheduling & Execution Priority Pipeline

When a worker searches for tasks, it executes the following 4-stage pipeline:
1. **Local Deque (LIFO)**: Pop most recent task from local Chase-Lev deque (warm cache).
2. **Work-Stealing (FIFO)**: Pick pseudo-random or round-robin victim worker and attempt `steal()` from top.
3. **Global MPMC Queue**: Attempt non-blocking `cobalt_lfqueue_dequeue()` to consume tasks submitted by external threads.
4. **Adaptive Idle Backoff**:
   - Spin for 32 iterations with `cobalt_thread_yield()` / CPU PAUSE hint.
   - Atomically increment `idle_workers`.
   - Wait on `cobalt_cond_wait(&pool->idle_cond, &pool->idle_mutex)`.

---

### 3.3 Concurrency Primitives

#### 1. Asynchronous Submit (`cobalt_threadpool_submit`)
- Detects whether calling thread is an active worker via `_Thread_local cobalt_worker_t *tl_worker`.
- If worker: pushes directly into `tl_worker->local_deque` (0 allocations, 0 locks).
- If external thread: packages task into global `cobalt_lfqueue`.
- If `idle_workers > 0`, signals `idle_cond` to wake an idle worker.

#### 2. Synchronization Barrier (`cobalt_waitgroup_t`)
```c
struct cobalt_waitgroup {
    atomic_int_fast64_t counter; /**< Active tasks counter */
    cobalt_mutex_t     *mutex;   /**< Synchronization mutex */
    cobalt_cond_t      *cond;    /**< Zero-counter condition variable */
};
```
- `cobalt_waitgroup_add(wg, delta)`: Atomic increment.
- `cobalt_waitgroup_done(wg)`: Atomic decrement; if 0, broadcasts `cond`.
- `cobalt_waitgroup_wait(wg)`:
  - If called from a Worker thread, performs **Help-First Waiting** (processes local tasks and steals) until `counter == 0`, eliminating thread starvation deadlock.
  - If called from external thread, blocks on condition variable until 0.

#### 3. Parallel For (`cobalt_threadpool_parallel_for`)
- Partitions range $[start, end)$ into $K$ chunks of size `chunk_size`.
- Default chunk size: $\max(1, (end - start) / (num\_workers \times 4))$.
- Dispatches chunks using an internal `cobalt_waitgroup_t`.
- Caller thread participates in chunk execution, eliminating thread idle waste.

---

## 4. Public API Specification (`include/cobalt/module/threadpool.h`)

```c
#ifndef COBALT_MODULE_THREADPOOL_H
#define COBALT_MODULE_THREADPOOL_H

#include "cobalt/memory/allocator.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cobalt_task_fn_t)(void *arg);
typedef void (*cobalt_parallel_for_fn_t)(size_t index, void *arg);

typedef struct {
    size_t              num_threads;       /**< 0 = auto-detect CPU cores */
    size_t              local_queue_size;  /**< Default 1024 (power of 2) */
    size_t              global_queue_size; /**< Default 4096 (power of 2) */
    cobalt_allocator_t *alloc;             /**< NULL = system allocator */
} cobalt_threadpool_config_t;

typedef struct cobalt_threadpool cobalt_threadpool_t;

/* Pool Lifecycle */
cobalt_threadpool_t *cobalt_threadpool_create(size_t num_threads);
cobalt_threadpool_t *cobalt_threadpool_create_with_config(const cobalt_threadpool_config_t *config);
void                 cobalt_threadpool_destroy(cobalt_threadpool_t *pool);

/* Task Dispatch & Parallel Execution */
int cobalt_threadpool_submit(cobalt_threadpool_t *pool, cobalt_task_fn_t fn, void *arg);
int cobalt_threadpool_parallel_for(cobalt_threadpool_t *pool,
                                   size_t               start,
                                   size_t               end,
                                   size_t               chunk_size,
                                   cobalt_parallel_for_fn_t fn,
                                   void                *arg);
void cobalt_threadpool_wait_idle(cobalt_threadpool_t *pool);

/* Introspection */
size_t cobalt_threadpool_thread_count(const cobalt_threadpool_t *pool);
size_t cobalt_threadpool_active_tasks(const cobalt_threadpool_t *pool);

/* WaitGroup Barrier */
typedef struct cobalt_waitgroup cobalt_waitgroup_t;
cobalt_waitgroup_t *cobalt_waitgroup_create(void);
void cobalt_waitgroup_add(cobalt_waitgroup_t *wg, int64_t delta);
void cobalt_waitgroup_done(cobalt_waitgroup_t *wg);
void cobalt_waitgroup_wait(cobalt_waitgroup_t *wg);
void cobalt_waitgroup_destroy(cobalt_waitgroup_t *wg);

#ifdef __cplusplus
}
#endif

#endif /* COBALT_MODULE_THREADPOOL_H */
```

---

## 5. Verification & Benchmark Strategy

### 5.1 Unit Tests (`tests/unit/test_threadpool.c`)
- **Basic Dispatch**: Submit multiple tasks with diverse payloads and verify execution.
- **Stress Concurrency**: 100,000 tasks submitted concurrently across all workers to verify 0 dropped tasks, 0 memory leaks, and 0 deadlocks.
- **WaitGroup Synchronization**: Multi-threaded fan-out and barrier rendezvous testing.
- **Parallel For**: Parallel array summation across 1,000,000 elements comparing against serial baseline.
- **Help-First Deadlock Elimination**: Nested task submission within workers waiting on WaitGroups.
- **Lifecycle & Auto-Detect**: Creation with `num_threads = 0` (auto-detect) and graceful shutdown verification.

### 5.2 Benchmark Suite (`tests/benchmark/benchmark_threadpool.c`)
- Task throughput measurement (Million tasks/second).
- Parallel For multi-core speedup ratio across 1, 2, 4, 8+ threads.
