# Cobalt Parallel Algorithms & TaskGraph Engine Design Specification

- **Module:** Parallel Algorithms & Directed Acyclic Graph Scheduler (`include/cobalt/module/`, `src/module/`)
- **Headers:** `parallel_sort.h`, `taskgraph.h`
- **Date:** 2026-10-17
- **Version:** Cobalt v2.6.0-dev
- **Status:** Approved / Ready for Implementation Planning

---

## 1. Executive Summary & Goals

Following the successful implementation of the Layer 3 sorting engine (PDQSort/Heapsort/TimSort), the Layer 4 stream buffer (`cobalt_buffer_t`), the lock-free MPMC queue (`cobalt_lfqueue_t`), and the Layer 2 work-stealing thread pool (`cobalt_threadpool_t`), this milestone expands Cobalt's concurrent computing capabilities into **high-performance parallel algorithms** and **event-driven task graph workflow orchestration**.

### Primary Goals
1. **Parallel Pattern-Defeating Quicksort (`cobalt_parallel_qsort`)**:
   - Zero-heap-allocation, multi-core divide-and-conquer sorting.
   - Leverages `cobalt_waitgroup_t` Help-First work-stealing to eliminate recursive fork-join thread starvation deadlocks.
   - Dynamic adaptive cutoff heuristic falling back to in-place sequential `cobalt_qsort`.
   - Linear speedup scaling on multi-core architectures ($S_p \approx 0.7 \times P$ on modern x86_64/ARM64).
2. **Parallel Adaptive Stable MergeSort (`cobalt_parallel_stable_sort`)**:
   - Guaranteed 100% stable sorting on arbitrary composite structures.
   - Two-phase execution: parallel chunk pre-sorting via `cobalt_threadpool_parallel_for` followed by parallel hierarchical tree merging.
   - Support for custom allocator injection (`cobalt_parallel_stable_sort_with_alloc`).
3. **Reusable Directed Acyclic Graph Scheduler (`cobalt_taskgraph_t`)**:
   - General-purpose DAG task dependency management.
   - $O(V+E)$ Kahn's algorithm cycle detection and topological compilation (`cobalt_taskgraph_compile`).
   - Purely event-driven, push-based scheduling via atomic remaining in-degree counters (`atomic_size_t remaining_in_degree`) without centralized polling.
   - Zero dynamic memory allocation on re-execution: compile once, run repeatedly.
   - Synchronous wait via Help-First WaitGroup (`cobalt_taskgraph_run_and_wait`) or asynchronous launch (`cobalt_taskgraph_run`).

---

## 2. Layer Architecture & Dependency Invariants

Cobalt enforces a strict zero-circular, upward-only layer hierarchy:
```
L8 (platform/atomic) -> L7 (memory/runtime) -> L6 (object/class) -> L5 (interfaces)
  -> L4 (containers: vector, buffer, lfqueue) -> L3 (algorithms: sort, functional)
  -> L2 (modules: json, eventloop, threadpool, parallel_sort, taskgraph)
  -> L1 (examples, benchmarks)
```

```
+-------------------------------------------------------------------------+
| L1: Applications & Benchmarks                                           |
|     (benchmark_parallel_sort, benchmark_taskgraph, bench_runner)        |
+-------------------------------------------------------------------------+
                                    ▲
                                    │ (depends on)
+-------------------------------------------------------------------------+
| L2: Module Layer                                                        |
|   +---------------------------------+  +-----------------------------+  |
|   |  parallel_sort.h/.c             |  |  taskgraph.h/.c             |  |
|   |  - cobalt_parallel_qsort        |  |  - cobalt_taskgraph_t       |  |
|   |  - cobalt_parallel_stable_sort  |  |  - Kahn's cycle detection   |  |
|   +---------------------------------+  +-----------------------------+  |
|                                   │      │                              |
|                                   ▼      ▼                              |
|                   +----------------------------------+                  |
|                   |  threadpool.h/.c                 |                  |
|                   |  - cobalt_threadpool_t           |                  |
|                   |  - cobalt_waitgroup_t            |                  |
|                   |  - Help-First Work Stealing      |                  |
|                   +----------------------------------+                  |
+-------------------------------------------------------------------------+
                                    ▲
                                    │ (depends on)
+-------------------------------------------------------------------------+
| L3: Algorithm Layer: sort.h (cobalt_qsort, cobalt_stable_sort, partition)|
+-------------------------------------------------------------------------+
| L4: Container Layer: vector.h, buffer.h, lfqueue.h                      |
+-------------------------------------------------------------------------+
| L6-L8: Object, Allocator, Error, Platform Atomics                       |
+-------------------------------------------------------------------------+
```

### Invariant Rules
- `parallel_sort.h` and `taskgraph.h` reside exclusively in `include/cobalt/module/`.
- `parallel_sort.c` and `taskgraph.c` reside in `src/module/`.
- They depend downstream on Layer 3 `cobalt/algorithm/sort.h`, Layer 4 `cobalt/container/vector.h`, and sibling Layer 2 `cobalt/module/threadpool.h`.
- No circular dependencies exist. Layer 3 `sort.h` remains completely independent of threading and threadpools.

---

## 3. Parallel Sorting Engine (`parallel_sort.h` / `parallel_sort.c`)

### 3.1 Public Header Declaration

```c
#ifndef COBALT_MODULE_PARALLEL_SORT_H
#define COBALT_MODULE_PARALLEL_SORT_H

#include "cobalt/algorithm/sort.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/module/threadpool.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief In-place parallel pattern-defeating quicksort
 * @param pool Thread pool for task dispatch (falls back to sequential if NULL or 1 core)
 * @param base Pointer to array memory
 * @param nmemb Number of elements
 * @param size Element size in bytes
 * @param compar Element comparison function
 * @note Unstable sort, zero heap allocations, O(N log N) worst-case time complexity.
 */
void cobalt_parallel_qsort(cobalt_threadpool_t *pool,
                           void                *base,
                           size_t               nmemb,
                           size_t               size,
                           compare_func_t       compar);

/**
 * @brief In-place parallel quicksort with customizable sub-task cutoff threshold
 * @param pool Thread pool
 * @param base Pointer to array memory
 * @param nmemb Number of elements
 * @param size Element size in bytes
 * @param compar Element comparison function
 * @param cutoff Sub-array size threshold below which sorting switches to sequential PDQSort
 */
void cobalt_parallel_qsort_with_cutoff(cobalt_threadpool_t *pool,
                                       void                *base,
                                       size_t               nmemb,
                                       size_t               size,
                                       compare_func_t       compar,
                                       size_t               cutoff);

/**
 * @brief Parallel adaptive stable sort (TimSort-lite)
 * @param pool Thread pool for concurrent chunk sorting and hierarchical merging
 * @param base Pointer to array memory
 * @param nmemb Number of elements
 * @param size Element size in bytes
 * @param compar Element comparison function
 * @note 100% stable sorting guarantee.
 */
void cobalt_parallel_stable_sort(cobalt_threadpool_t *pool,
                                 void                *base,
                                 size_t               nmemb,
                                 size_t               size,
                                 compare_func_t       compar);

/**
 * @brief Parallel adaptive stable sort with custom allocator for scratch merge memory
 * @param pool Thread pool
 * @param base Pointer to array memory
 * @param nmemb Number of elements
 * @param size Element size in bytes
 * @param compar Element comparison function
 * @param alloc Custom memory allocator (falls back to system allocator if NULL)
 */
void cobalt_parallel_stable_sort_with_alloc(cobalt_threadpool_t *pool,
                                            void                *base,
                                            size_t               nmemb,
                                            size_t               size,
                                            compare_func_t       compar,
                                            cobalt_allocator_t  *alloc);

#ifdef __cplusplus
}
#endif

#endif /* COBALT_MODULE_PARALLEL_SORT_H */
```

---

### 3.2 Parallel PDQSort Algorithm Flow

```
                      [nmemb Elements]
                             |
                   Hoare-style Partition
                     /              \
         Left Partition            Right Partition
     [0, split) >= cutoff?       [split, nmemb)
            /          \                |
          YES           NO          Continue in
          /              \          current thread
Submit Task to Pool    Sequential
(cobalt_threadpool)    cobalt_qsort
```

1. **Cutoff Heuristic**:
   - If `pool == NULL`, `cobalt_threadpool_thread_count(pool) <= 1`, or `nmemb <= 4096`, directly invoke sequential `cobalt_qsort`.
   - Default adaptive cutoff:
     $$\text{cutoff} = \max\left(4096, \frac{N}{4 \times \text{num\_threads}}\right)$$
2. **Task Payload Structure**:
   ```c
   typedef struct {
       cobalt_threadpool_t *pool;
       char                *base;
       size_t               nmemb;
       size_t               size;
       compare_func_t       compar;
       size_t               cutoff;
       cobalt_waitgroup_t  *wg;
   } parallel_qsort_task_t;
   ```
3. **Partition & Fork-Join**:
   - The array is partitioned using the in-place multi-word Hoare partitioner `cobalt_partition`.
   - `cobalt_waitgroup_add(wg, 1)` increments the barrier.
   - The left slice is enqueued to `pool` using `cobalt_threadpool_submit`. When submitted by a worker thread, this pushes directly into the local Chase-Lev deque with zero dynamic allocations.
   - The calling worker thread immediately continues recursing into the right slice.
   - When a slice drops to $\le \text{cutoff}$, it is sorted synchronously with `cobalt_qsort`.
   - Upon completing its branch, the task calls `cobalt_waitgroup_done(wg)`.
4. **Deadlock Elimination**:
   - The root caller invokes `cobalt_waitgroup_wait(wg)`.
   - Because `cobalt_waitgroup_wait` implements **Help-First Work-Stealing**, worker threads never block or put OS threads to sleep. They actively pop local tasks and steal pending branches from other workers, completely eliminating thread starvation deadlocks regardless of recursion depth.

---

### 3.3 Parallel Stable Sort Algorithm Flow

```
Stage 1: Concurrent Chunk Sort
[ Chunk 0 ] [ Chunk 1 ] [ Chunk 2 ] [ Chunk 3 ]  (cobalt_threadpool_parallel_for)
     \           /           \           /
Stage 2: Pairwise Parallel Tree Merge
     [ Merge 0,1 ]           [ Merge 2,3 ]        (level-by-level parallel merge)
           \                       /
Stage 3: Final Parallel Merge
              [ Final Merged Array ]
```

1. **Chunk Decomposition**:
   - Compute number of chunks $K = 2^{\lceil \log_2(2 \times \text{threads}) \rceil}$, capped at $64$.
   - Slice array into $K$ equal contiguous chunks.
2. **Phase 1: Parallel Sorting**:
   - Use `cobalt_threadpool_parallel_for` to execute sequential `cobalt_stable_sort` across all $K$ chunks concurrently.
3. **Phase 2: Hierarchical Binary Merge Tree**:
   - Allocate a single shared scratch buffer of size $N \times \text{size}$ using `alloc` or system allocator.
   - Tree merge across $\log_2 K$ levels:
     - At each level, merge pairs of adjacent chunks $(2i, 2i+1)$ into scratch space, then copy back (or ping-pong buffers).
     - Each pair merge is submitted as a concurrent task synchronized by a level `cobalt_waitgroup_t`.
     - During merge, strict stability is enforced: if `compar(A[i], B[j]) <= 0`, element from chunk $A$ is picked first.
4. **Clean Termination**:
   - Scratch buffer is released via `alloc->free` or `free()`.
   - Guarantees 100% stable sorting with linear multi-core speedup.

---

## 4. Directed Acyclic Graph Scheduler (`taskgraph.h` / `taskgraph.c`)

### 4.1 Public Header Declaration

```c
#ifndef COBALT_MODULE_TASKGRAPH_H
#define COBALT_MODULE_TASKGRAPH_H

#include "cobalt/memory/allocator.h"
#include "cobalt/module/threadpool.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t cobalt_task_id_t;
#define COBALT_INVALID_TASK_ID ((cobalt_task_id_t)-1)

typedef struct cobalt_taskgraph cobalt_taskgraph_t;

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create a new empty TaskGraph using the system allocator
 * @return Newly allocated TaskGraph, or NULL on failure
 */
cobalt_taskgraph_t *cobalt_taskgraph_create(void);

/**
 * @brief Create a new TaskGraph using a custom memory allocator
 * @param alloc Custom allocator
 * @return Newly allocated TaskGraph, or NULL on failure
 */
cobalt_taskgraph_t *cobalt_taskgraph_create_with_alloc(cobalt_allocator_t *alloc);

/**
 * @brief Destroy a TaskGraph and release all graph data structures
 * @param graph TaskGraph instance. Safe with NULL.
 */
void cobalt_taskgraph_destroy(cobalt_taskgraph_t *graph);

/* -------------------------------------------------------------------------- */
/* Graph Topology Construction                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Add an executable task node to the graph
 * @param graph TaskGraph instance
 * @param fn Task callback function
 * @param ctx Context pointer passed to fn
 * @return Unique node ID, or COBALT_INVALID_TASK_ID on failure
 */
cobalt_task_id_t cobalt_taskgraph_add_node(cobalt_taskgraph_t *graph,
                                           cobalt_task_fn_t    fn,
                                           void               *ctx);

/**
 * @brief Add a directed dependency edge: from_id must finish before to_id starts
 * @param graph TaskGraph instance
 * @param from_id Predecessor task node ID
 * @param to_id Successor task node ID
 * @return 0 on success, -1 on invalid node IDs or if graph is already compiled
 */
int cobalt_taskgraph_add_edge(cobalt_taskgraph_t *graph,
                              cobalt_task_id_t    from_id,
                              cobalt_task_id_t    to_id);

/**
 * @brief Validate, cycle-check, and compile the TaskGraph topology
 * @details Runs Kahn's topological sort algorithm to verify acyclicity.
 *          If a cycle is detected, returns -1 and sets COBALT_ERROR_INVALID_ARGUMENT.
 *          Extracts and caches root tasks (static in-degree == 0).
 * @param graph TaskGraph instance
 * @return 0 on success, -1 if cycle detected or invalid graph
 */
int cobalt_taskgraph_compile(cobalt_taskgraph_t *graph);

/* -------------------------------------------------------------------------- */
/* Execution & Reusable Dispatch                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Asynchronously start execution of the TaskGraph
 * @param graph Compiled TaskGraph instance
 * @param pool Thread pool for task execution
 * @return 0 on success, -1 if graph is not compiled or invalid
 */
int cobalt_taskgraph_run(cobalt_taskgraph_t *graph, cobalt_threadpool_t *pool);

/**
 * @brief Start TaskGraph execution and block until all nodes have completed
 * @details Uses Help-First WaitGroup so the calling thread assists in execution,
 *          eliminating deadlocks and reducing latency.
 * @param graph Compiled TaskGraph instance
 * @param pool Thread pool for task execution
 * @return 0 on success, -1 on failure
 */
int cobalt_taskgraph_run_and_wait(cobalt_taskgraph_t *graph, cobalt_threadpool_t *pool);

/**
 * @brief Reset dynamic runtime counters to allow graph re-execution
 * @param graph TaskGraph instance
 */
void cobalt_taskgraph_reset(cobalt_taskgraph_t *graph);

/**
 * @brief Get total number of task nodes in the graph
 * @param graph TaskGraph instance
 * @return Number of nodes, or 0 if NULL
 */
size_t cobalt_taskgraph_node_count(const cobalt_taskgraph_t *graph);

#ifdef __cplusplus
}
#endif

#endif /* COBALT_MODULE_TASKGRAPH_H */
```

---

### 4.2 Data Structures & Memory Layout

```c
typedef struct {
    cobalt_task_id_t     id;                  /* Zero-indexed node ID */
    cobalt_task_fn_t     fn;                  /* User function pointer */
    void                *ctx;                 /* User context pointer */
    size_t               static_in_degree;    /* Immutable dependency count from compile */
    _Atomic(size_t)      remaining_in_degree; /* Mutable dynamic in-degree */
    cobalt_vector_t     *successors;          /* Dynamic array of successor cobalt_task_id_t */
} cobalt_task_node_t;

struct cobalt_taskgraph {
    cobalt_vector_t    *nodes;       /* Array of cobalt_task_node_t* */
    cobalt_vector_t    *root_nodes;  /* Array of cobalt_task_id_t for nodes with in_degree == 0 */
    bool                compiled;    /* True if compile succeeded and graph is locked */
    cobalt_waitgroup_t *wg;          /* Internal barrier for execution synchronization */
    cobalt_threadpool_t*bound_pool;  /* Active thread pool during execution */
    cobalt_allocator_t *alloc;       /* Memory allocator */
};
```

---

### 4.3 Kahn's Algorithm for Topological Cycle Detection

During `cobalt_taskgraph_compile(graph)`:
1. Allocate temporary array `size_t temp_in_degree[N]`.
2. For each node $i \in [0, N-1]$, initialize `temp_in_degree[i] = node[i]->static_in_degree`.
3. Enqueue all nodes with `temp_in_degree[i] == 0` into a FIFO queue.
4. Process queue:
   - Pop node $u$, increment `visited_count`.
   - For each successor $v \in u\text{.successors}$:
     - Decrement `temp_in_degree[v]--`.
     - If `temp_in_degree[v] == 0`, enqueue $v$.
5. Verification:
   - If `visited_count == N`, graph is a valid DAG. Cache root nodes in `graph->root_nodes`, mark `graph->compiled = true`, return 0.
   - If `visited_count < N`, a cycle exists! Set error `COBALT_ERROR_INVALID_ARGUMENT`, return -1.

---

### 4.4 Event-Driven Atomic In-Degree Dispatch Flow

```
                      [ Root 0 ]       [ Root 1 ]
                            \             /
                             \           /
               atomic_fetch_sub(&succ->remaining_in_degree, 1)
                                   |
                                 == 1?
                                /     \
                              YES      NO
                              /         \
                 Submit succ to pool     Wait for other predecessors
```

1. **Invocation**:
   - `cobalt_taskgraph_run_and_wait(graph, pool)` resets dynamic state:
     - For each node $i$: `atomic_store(&node[i]->remaining_in_degree, node[i]->static_in_degree)`.
     - Initializes `cobalt_waitgroup_add(graph->wg, N)`.
   - For each root node in `graph->root_nodes`, dispatches a task wrapper:
     ```c
     cobalt_threadpool_submit(pool, taskgraph_worker_wrapper, wrapper_arg);
     ```
2. **Worker Wrapper Execution**:
   ```c
   static void taskgraph_worker_wrapper(void *arg) {
       node_task_arg_t *task_arg = (node_task_arg_t *)arg;
       cobalt_taskgraph_t *graph = task_arg->graph;
       cobalt_task_node_t *node = task_arg->node;

       // 1. Run user code
       node->fn(node->ctx);

       // 2. Decrement internal WaitGroup barrier
       cobalt_waitgroup_done(graph->wg);

       // 3. Cascading successor activation
       size_t succ_count = cobalt_vector_size(node->successors);
       for (size_t i = 0; i < succ_count; i++) {
           cobalt_task_id_t succ_id = (cobalt_task_id_t)(uintptr_t)cobalt_vector_get(node->successors, i);
           cobalt_task_node_t *succ = (cobalt_task_node_t *)cobalt_vector_get(graph->nodes, succ_id);

           size_t prev_deg = atomic_fetch_sub_explicit(&succ->remaining_in_degree, 1, memory_order_acq_rel);
           if (prev_deg == 1) {
               // All predecessors have finished! Dispatch immediately!
               cobalt_threadpool_submit(graph->bound_pool, taskgraph_worker_wrapper, ...);
           }
       }
   }
   ```
3. **Zero Polling & Zero-Reallocation Reusability**:
   - Successors are triggered directly upon the final predecessor's atomic decrement.
   - Calling `cobalt_taskgraph_run_and_wait` in a tight loop runs with zero heap allocations and sub-microsecond transition latency.

---

## 5. Verification & Testing Strategy

### 5.1 Unit Tests (`tests/unit/test_parallel_sort.c`)
- **Random Distribution**: 500,000 integers sorted with `cobalt_parallel_qsort` across 4-8 threads.
- **Adversarial Distributions**: Strictly ascending, strictly descending, organ pipe, and all-equal arrays.
- **100% Stability Verification**: 100,000 elements with duplicate keys (`struct { int key; int original_index; }`), verifying key ordering and strict original index preservation.
- **Custom Allocator**: `cobalt_parallel_stable_sort_with_alloc` using mock allocator.
- **Boundary Conditions**: NULL pointers, 0 elements, 1 element, tiny arrays below cutoff, NULL pool fallback.

### 5.2 Unit Tests (`tests/unit/test_taskgraph.c`)
- **Linear Chain**: $A \to B \to C \to D$ verifying strict serial execution order.
- **Diamond Graph**: $A \to B, A \to C, B \to D, C \to D$ verifying concurrent execution of $B$ and $C$, and barrier synchronization before $D$.
- **Complex Forest**: Multiple independent DAGs with wide fan-out (1 to 100) and fan-in (100 to 1).
- **Cycle Detection**: Self-cycles ($A \to A$), 2-node cycles ($A \to B \to A$), and multi-node cycles ($A \to B \to C \to A$) verified to fail compile with error code.
- **Loop Reusability**: Run the same compiled graph 100 consecutive times, asserting correct counter resets, zero memory leaks, and 100% completion.

### 5.3 Benchmarks (`tests/benchmark/`)
- `benchmark_parallel_sort.c`:
  - 1,000,000 and 10,000,000 integers.
  - Measures execution time and computes multi-core Speedup Factor ($S_p = T_{\text{sequential}} / T_{\text{parallel}}$).
  - Target: $S_p \ge 3.0\times$ on 4 cores, $S_p \ge 5.0\times$ on 8 cores.
- `benchmark_taskgraph.c`:
  - Measures latency and scheduling throughput for 10,000 fine-grained task nodes across thread pools.
  - Target: $\ge 1,000,000$ DAG tasks/sec dispatch throughput.
- Integrated into `tests/benchmark/bench_runner.c` with `--json` output.

---

## 6. Spec Self-Review Checklist

1. **Placeholder Scan**: Zero "TBD", "TODO", or vague requirements. All function signatures and behaviors are explicitly defined.
2. **Internal Consistency**: Matches Layer 2 conventions, follows `AGENTS.md` (C11, snake_case, 4-space indent, right-aligned pointers `int *ptr`).
3. **Scope Check**: Covers parallel quicksort, parallel stable sort, and reusable task graph scheduler as requested.
4. **Ambiguity Check**: Error conditions, cutoff heuristics, memory ownership, and deadlock-free guarantees are mathematically and architecturally unambiguous.
