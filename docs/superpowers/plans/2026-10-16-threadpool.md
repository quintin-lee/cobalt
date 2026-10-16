# Cobalt High-Performance Work-Stealing Thread Pool Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a high-performance, zero-dependency C11 work-stealing thread pool and asynchronous task scheduler in Layer 2 Modules (`include/cobalt/module/threadpool.h`, `src/module/threadpool.c`), featuring Chase-Lev SPMC deques, global MPMC queue fallback, WaitGroup synchronization barriers with help-first deadlock prevention, and dynamic chunked parallel loops.

**Architecture:**
- **Hybrid Scheduling Engine**: External callers submit to a global lock-free MPMC queue (`cobalt_lfqueue_t`), while Worker threads push to their private Chase-Lev SPMC deque.
- **Cache-Optimized Work-Stealing**: Workers pop their own tasks in LIFO order (warm L1/L2 cache), and steal from victim workers in FIFO order (coarse-grained work).
- **Zero-Allocation Hot-Path**: Deque ring slots embed task structures directly, avoiding heap allocations during task dispatch.
- **Adaptive Idle Backoff**: 3-stage backoff (short spin with pause/yield, atomic idle registration, condition variable wait) preventing CPU burning while ensuring sub-microsecond wakeup latencies.
- **Help-First Waiting**: When a worker waits on a WaitGroup, it actively steals and processes tasks instead of blocking, completely eliminating thread starvation deadlocks.

**Tech Stack:** C11 (`stdatomic.h`, `_Thread_local`, `_Alignas`), POSIX pthreads / Win32 threads via `cobalt/platform/thread.h`, `cobalt/container/lfqueue.h`, CMake, CTest, clang-format, ASan.

---

## File Structure

### Files to Create
- `include/cobalt/module/threadpool.h`: Public API header with threadpool, task, waitgroup, and parallel-for declarations.
- `src/module/threadpool.c`: Core implementation of Chase-Lev deque, worker thread lifecycle, work-stealing, waitgroup, and parallel-for.
- `tests/unit/test_threadpool.c`: Comprehensive unit test suite covering task dispatch, concurrency stress, waitgroups, parallel-for, and deadlock prevention.
- `tests/benchmark/benchmark_threadpool.c`: Standalone benchmark measuring task throughput (tasks/second) and multi-core scaling.

### Files to Modify
- `CMakeLists.txt`: Add `src/module/threadpool.c` to `SOURCES`.
- `include/cobalt/cobalt.h`: Include `"cobalt/module/threadpool.h"`.
- `tests/CMakeLists.txt`: Register `test_threadpool` and `benchmark_threadpool`.
- `tests/test_runner.c`: Wire `test_threadpool` into the unit test registry.
- `tests/benchmark/bench_runner.c`: Wire threadpool benchmarks into the unified benchmark runner.

---

## Task 1: Public Header & Chase-Lev SPMC Work-Stealing Deque

**Files:**
- Create: `include/cobalt/module/threadpool.h`
- Create: `src/module/threadpool.c:1-120`
- Create: `tests/unit/test_threadpool.c:1-80`
- Modify: `CMakeLists.txt:80-95`
- Modify: `include/cobalt/cobalt.h`

- [ ] **Step 1: Create `include/cobalt/module/threadpool.h`**

Define the public interface with Doxygen documentation:

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
int  cobalt_threadpool_submit(cobalt_threadpool_t *pool, cobalt_task_fn_t fn, void *arg);
int  cobalt_threadpool_parallel_for(cobalt_threadpool_t *pool,
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

- [ ] **Step 2: Implement Chase-Lev Deque in `src/module/threadpool.c`**

Implement Chase-Lev SPMC deque with `push`, `pop`, and `steal`:

```c
#include "cobalt/module/threadpool.h"
#include "cobalt/container/lfqueue.h"
#include "cobalt/platform/thread.h"
#include "cobalt/platform/atomic.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    cobalt_task_fn_t fn;
    void            *arg;
} cobalt_task_t;

typedef struct {
    cobalt_task_t      *buffer;
    size_t              capacity;
    size_t              mask;
    _Alignas(64) atomic_size_t top;
    _Alignas(64) atomic_size_t bottom;
} chase_lev_deque_t;

static int chase_lev_init(chase_lev_deque_t *q, size_t capacity, cobalt_allocator_t *alloc)
{
    if (capacity < 2) capacity = 2;
    /* Round up to power of 2 */
    size_t cap = 1;
    while (cap < capacity) cap <<= 1;

    q->capacity = cap;
    q->mask = cap - 1;
    atomic_init(&q->top, 0);
    atomic_init(&q->bottom, 0);

    size_t buf_bytes = cap * sizeof(cobalt_task_t);
    q->buffer = alloc ? (cobalt_task_t *)alloc->alloc(alloc, buf_bytes)
                      : (cobalt_task_t *)malloc(buf_bytes);
    return q->buffer ? 0 : -1;
}

static void chase_lev_destroy(chase_lev_deque_t *q, cobalt_allocator_t *alloc)
{
    if (q->buffer) {
        if (alloc) alloc->free(alloc, q->buffer);
        else free(q->buffer);
        q->buffer = NULL;
    }
}

static int chase_lev_push(chase_lev_deque_t *q, cobalt_task_t task)
{
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
    if (b - t >= q->capacity) {
        return -1; /* Deque full */
    }
    q->buffer[b & q->mask] = task;
    atomic_store_explicit(&q->bottom, b + 1, memory_order_release);
    return 0;
}

static int chase_lev_pop(chase_lev_deque_t *q, cobalt_task_t *out_task)
{
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    if (b == 0) return -1;
    b--;
    atomic_store_explicit(&q->bottom, b, memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);

    size_t t = atomic_load_explicit(&q->top, memory_order_relaxed);
    if (t <= b) {
        *out_task = q->buffer[b & q->mask];
        if (t == b) {
            /* Last element: race against stealers */
            size_t expected_t = t;
            if (!atomic_compare_exchange_strong_explicit(&q->top, &expected_t, t + 1,
                                                         memory_order_seq_cst,
                                                         memory_order_relaxed)) {
                atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
                return -1;
            }
            atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
        }
        return 0;
    } else {
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
        if (atomic_compare_exchange_weak_explicit(&q->top, &t, t + 1,
                                                  memory_order_seq_cst,
                                                  memory_order_relaxed)) {
            return 0;
        }
    }
    return -1;
}
```

- [x] **Step 3: Write tests for Chase-Lev Deque in `tests/unit/test_threadpool.c`**

Test single-threaded push/pop LIFO order, wrap-around, and multi-threaded stealer contention.

- [x] **Step 4: Update `CMakeLists.txt` and `include/cobalt/cobalt.h`**

Add `src/module/threadpool.c` to `SOURCES` in `CMakeLists.txt`, and include `"cobalt/module/threadpool.h"` in `include/cobalt/cobalt.h`.

- [x] **Step 5: Verify build & commit**

```bash
cmake --build build -j4
git add include/cobalt/module/threadpool.h src/module/threadpool.c tests/unit/test_threadpool.c CMakeLists.txt include/cobalt/cobalt.h
git commit -m "feat(threadpool): ✨ implement Chase-Lev work-stealing deque"
```

---

## Task 2: Core Thread Pool Lifecycle & Worker Thread Engine

**Files:**
- Modify: `src/module/threadpool.c`
- Modify: `tests/unit/test_threadpool.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_runner.c`

- [x] **Step 1: Implement thread pool structures and lifecycle in `src/module/threadpool.c`**

Define `cobalt_worker_t` and `cobalt_threadpool_t`:
```c
typedef struct cobalt_worker {
    cobalt_thread_t           thread_handle;
    size_t                    worker_id;
    struct cobalt_threadpool *pool;
    chase_lev_deque_t         local_deque;
    _Alignas(64) atomic_uint_fast64_t tasks_executed;
} cobalt_worker_t;

struct cobalt_threadpool {
    size_t                  num_workers;
    cobalt_worker_t        *workers;
    cobalt_lfqueue_t       *global_queue;
    cobalt_allocator_t     *alloc;
    atomic_bool             running;
    atomic_size_t           active_tasks;
    atomic_size_t           idle_workers;
    cobalt_mutex_t         *idle_mutex;
    cobalt_cond_t          *idle_cond;
    cobalt_mutex_t         *drain_mutex;
    cobalt_cond_t          *drain_cond;
};

static _Thread_local cobalt_worker_t *tl_current_worker = NULL;
```

Implement auto-detect CPU cores, `cobalt_threadpool_create`, `cobalt_threadpool_create_with_config`, and `cobalt_threadpool_destroy`.
Implement worker thread main loop executing the 4-tier pipeline:
1. `chase_lev_pop(&worker->local_deque, &task)` (LIFO)
2. `chase_lev_steal(&victim->local_deque, &task)` (FIFO round-robin)
3. `cobalt_lfqueue_dequeue(pool->global_queue, (void **)&task_ptr)`
4. Adaptive idle backoff (yield -> sleep on condvar)

- [x] **Step 2: Wire `test_threadpool` in `tests/CMakeLists.txt` and `tests/test_runner.c`**

- [x] **Step 3: Run test suite**

```bash
cmake --build build -j4 && ./build/tests/cobalt_test --filter threadpool
```
Expected: PASS

- [x] **Step 4: Commit changes**

```bash
git add src/module/threadpool.c tests/unit/test_threadpool.c tests/CMakeLists.txt tests/test_runner.c
git commit -m "feat(threadpool): ✨ implement core thread pool lifecycle and worker loop"
```

---

## Task 3: Task Submission & Adaptive Idle Backoff

**Files:**
- Modify: `src/module/threadpool.c`
- Modify: `tests/unit/test_threadpool.c`

- [x] **Step 1: Implement `cobalt_threadpool_submit` and `cobalt_threadpool_wait_idle`**

Implement zero-alloc routing:
- If `tl_current_worker != NULL`: attempt `chase_lev_push(&tl_current_worker->local_deque, task)`.
- If full or external thread: allocate task descriptor and enqueue to `pool->global_queue`.
- Check `idle_workers > 0`: if idle workers exist, call `cobalt_cond_signal(pool->idle_cond)`.
- Maintain `atomic_fetch_add(&pool->active_tasks, 1)`.
- When task finishes, decrement `active_tasks`. If 0, signal `drain_cond`.

- [x] **Step 2: Add high-throughput concurrency test in `tests/unit/test_threadpool.c`**

Submit 100,000 tasks across multiple concurrent producer threads and verify that every task is executed with atomic counter = 100,000.

- [x] **Step 3: Run test suite**

```bash
cmake --build build -j4 && ./build/tests/cobalt_test --filter threadpool
```
Expected: PASS

- [x] **Step 4: Commit changes**

```bash
git add src/module/threadpool.c tests/unit/test_threadpool.c
git commit -m "feat(threadpool): ✨ implement task submission and adaptive idle wakeup"
```

---

## Task 4: WaitGroup Synchronization Barrier with Help-First Work-Stealing

**Files:**
- Modify: `src/module/threadpool.c`
- Modify: `tests/unit/test_threadpool.c`

- [x] **Step 1: Implement `cobalt_waitgroup_t`**

Implement:
- `cobalt_waitgroup_create`
- `cobalt_waitgroup_add`
- `cobalt_waitgroup_done`
- `cobalt_waitgroup_wait`:
  - If `tl_current_worker != NULL`, execute **Help-First Work-Stealing**: actively poll local deque, steal from other workers, and dequeue global tasks while `atomic_load(&wg->counter) > 0`!
  - If external thread: wait on `cobalt_cond_wait(wg->cond, wg->mutex)`.
- `cobalt_waitgroup_destroy`

- [x] **Step 2: Add tests for WaitGroup and nested recursive tasks**

Test fork-join and recursive divide-and-conquer tasks without deadlock.

- [x] **Step 3: Run test suite**

```bash
cmake --build build -j4 && ./build/tests/cobalt_test --filter threadpool
```
Expected: PASS

- [x] **Step 4: Commit changes**

```bash
git add src/module/threadpool.c tests/unit/test_threadpool.c
git commit -m "feat(threadpool): ✨ implement waitgroup with help-first work-stealing"
```

---

## Task 5: Parallel For Loop Engine (`cobalt_threadpool_parallel_for`)

**Files:**
- Modify: `src/module/threadpool.c`
- Modify: `tests/unit/test_threadpool.c`

- [x] **Step 1: Implement `cobalt_threadpool_parallel_for`**

Divide range $[start, end)$ into balanced chunks. Submit chunk tasks to the pool with an internal WaitGroup, while caller thread immediately executes available chunks.

- [x] **Step 2: Add unit tests for `parallel_for`**

Compute parallel reduction (e.g. sum of 1,000,000 array elements) and verify against expected mathematical sum.

- [x] **Step 3: Run test suite**

```bash
cmake --build build -j4 && ./build/tests/cobalt_test --filter threadpool
```
Expected: PASS

- [x] **Step 4: Commit changes**

```bash
git add src/module/threadpool.c tests/unit/test_threadpool.c
git commit -m "feat(threadpool): ✨ implement parallel for chunking engine"
```

---

## Task 6: Thread Pool Benchmark Suite (`tests/benchmark/benchmark_threadpool.c`)

**Files:**
- Create: `tests/benchmark/benchmark_threadpool.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/benchmark/bench_runner.c`

- [x] **Step 1: Create `tests/benchmark/benchmark_threadpool.c`**

Measure:
1. Fire-and-forget task throughput (Million tasks/sec).
2. WaitGroup fan-out/fan-in throughput.
3. Parallel-for speedup across 1, 2, 4, 8 threads vs serial loop.
Print `"Benchmark completed\n"`.

- [x] **Step 2: Wire into `tests/CMakeLists.txt` and `bench_runner.c`**

- [x] **Step 3: Run benchmarks and verify output**

```bash
cmake --build build -j4
./build/tests/benchmark_threadpool
ctest --test-dir build --output-on-failure -R bench_threadpool
```
Expected: PASS

- [x] **Step 4: Commit changes**

```bash
git add tests/benchmark/benchmark_threadpool.c tests/CMakeLists.txt tests/benchmark/bench_runner.c
git commit -m "test(bench): ⚡ add benchmark suite for thread pool"
```

---

## Task 7: Full Test Suite, Sanitizer Check & Format Gate

**Files:**
- Repository-wide verification

- [x] **Step 1: Run clang-format check**

```bash
clang-format -i include/cobalt/module/threadpool.h src/module/threadpool.c tests/unit/test_threadpool.c tests/benchmark/benchmark_threadpool.c
clang-format --dry-run -Werror include/cobalt/module/threadpool.h src/module/threadpool.c tests/unit/test_threadpool.c tests/benchmark/benchmark_threadpool.c
```

- [x] **Step 2: Run all CTests and static analysis**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCOBALT_RUN_TIDY=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

- [x] **Step 3: Run AddressSanitizer & LeakSanitizer**

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCOBALT_ENABLE_ASAN=ON
cmake --build build-asan -j4
ctest --test-dir build-asan --output-on-failure -R threadpool
```

- [x] **Step 4: Commit finalization**

```bash
git commit -m "chore(threadpool): 🧹 format codebase and finalize Phase 2 thread pool" --allow-empty
```
