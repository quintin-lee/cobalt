# Cobalt Core Containers & Memory Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Robin Hood open addressing in HashMap & Set, convert Arena to non-moving chunked blocks for pointer stability, and add a bounded MPMC lock-free queue (`cobalt_lfqueue_t`) with complete unit tests and benchmarks.

**Architecture:** 
1. `cobalt_arena_t` switches to a singly-linked list of `arena_chunk_t` allocations, ensuring that previously returned pointers never move when the arena grows.
2. `cobalt_hashmap_t` switches from separate-chained `hashmap_node_t` pointers to a contiguous flat array of `hashmap_slot_t` with Robin Hood hashing (swap-on-insert), backward-shift tombstone-free deletion, and PSL early-exit lookup.
3. `cobalt_lfqueue_t` implements Dmitry Vyukov's bounded MPMC queue using C11 `stdatomic.h` with 64-byte cache-line padding on `head` and `tail` cursors to eliminate false sharing.

**Tech Stack:** C11, CMake 3.16+, CTest, Clang-Tidy, Clang-Format, POSIX threads / C11 atomics (`stdatomic.h`).

---

## File Structure

### Files to Create
- `include/cobalt/container/lfqueue.h` — Public API header for the bounded lock-free MPMC queue
- `src/container/lfqueue.c` — Implementation of `cobalt_lfqueue_t`
- `tests/unit/test_lfqueue.c` — Unit test suite for `cobalt_lfqueue_t`
- `tests/benchmark/benchmark_lfqueue.c` — Benchmark comparing single-threaded and multi-threaded throughput vs `cobalt_tsqueue`

### Files to Modify
- `src/memory/arena.c` — Replace single-buffer realloc with chunked linked list
- `tests/unit/test_arena.c` — Add pointer stability tests across chunk boundaries
- `src/container/hashmap.c` — Replace bucket-chain nodes with Robin Hood flat slot array
- `tests/unit/test_hashmap.c` — Add backward-shift collision and large-scale tests
- `include/cobalt/cobalt.h` — Include `cobalt/container/lfqueue.h`
- `CMakeLists.txt` — Add `src/container/lfqueue.c` to `COBALT_SOURCES`
- `tests/CMakeLists.txt` — Register `test_lfqueue` and `benchmark_lfqueue` targets
- `tests/benchmark/bench_runner.c` — Add `bench_lfqueue` to the aggregated benchmark runner

---

## Task 1: Arena Chunked Block Allocation (Pointer Stability)

**Files:**
- Modify: `tests/unit/test_arena.c`
- Modify: `src/memory/arena.c`

- [ ] **Step 1: Write pointer stability test in test_arena.c**

Add a test `test_arena_pointer_stability` in `tests/unit/test_arena.c`:
```c
TEST(arena, pointer_stability) {
    /* Create arena with a small initial size (128 bytes) */
    cobalt_arena_t *arena = cobalt_arena_create(128);
    TEST_ASSERT(arena != NULL);

    #define NUM_SLICES 64
    #define SLICE_SIZE 64
    void *ptrs[NUM_SLICES];

    /* Allocate 64 * 64 = 4096 bytes, forcing multiple chunk allocations */
    for (int i = 0; i < NUM_SLICES; i++) {
        ptrs[i] = cobalt_arena_alloc(arena, SLICE_SIZE);
        TEST_ASSERT(ptrs[i] != NULL);
        /* Fill with recognizable pattern */
        memset(ptrs[i], (i & 0xFF), SLICE_SIZE);
    }

    /* Verify all pointers remain stable and their memory content has not been corrupted */
    for (int i = 0; i < NUM_SLICES; i++) {
        unsigned char *bytes = (unsigned char *)ptrs[i];
        for (int j = 0; j < SLICE_SIZE; j++) {
            TEST_ASSERT(bytes[j] == (unsigned char)(i & 0xFF));
        }
    }

    /* Verify reset and re-allocation */
    cobalt_arena_reset(arena);
    void *new_ptr = cobalt_arena_alloc(arena, 64);
    TEST_ASSERT(new_ptr != NULL);

    cobalt_arena_destroy(arena);
    #undef NUM_SLICES
    #undef SLICE_SIZE
}
```

- [ ] **Step 2: Run test to see if existing implementation fails or corrupts memory**

Run: `cmake --build build --target cobalt_test && ./build/tests/cobalt_test --filter arena`

- [ ] **Step 3: Refactor src/memory/arena.c with chunked block architecture**

Replace the internals of `src/memory/arena.c` with chunk-based allocation:
```c
#include "cobalt/memory/arena.h"
#include "cobalt/memory/allocator.h"
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

#define COBALT_ARENA_MIN_CHUNK_SIZE 4096

typedef struct arena_chunk {
    struct arena_chunk *next;
    size_t              capacity;
    size_t              used;
    _Alignas(max_align_t) unsigned char data[];
} arena_chunk_t;

struct cobalt_arena {
    arena_chunk_t      *current;
    arena_chunk_t      *chunks;
    size_t              default_chunk_size;
    size_t              total_allocated;
    cobalt_allocator_t *alloc;
};

cobalt_arena_t *cobalt_arena_create(size_t initial_size)
{
    return cobalt_arena_create_with_allocator(initial_size, cobalt_allocator_get_system());
}

cobalt_arena_t *cobalt_arena_create_with_allocator(size_t initial_size, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_arena_t *arena = (cobalt_arena_t *)alloc->alloc(alloc, sizeof(cobalt_arena_t));
    if (!arena) {
        return NULL;
    }

    size_t chunk_cap = initial_size > 0 ? initial_size : COBALT_ARENA_MIN_CHUNK_SIZE;
    arena_chunk_t *chunk = (arena_chunk_t *)alloc->alloc(alloc, sizeof(arena_chunk_t) + chunk_cap);
    if (!chunk) {
        alloc->free(alloc, arena);
        return NULL;
    }

    chunk->next     = NULL;
    chunk->capacity = chunk_cap;
    chunk->used     = 0;

    arena->current            = chunk;
    arena->chunks             = chunk;
    arena->default_chunk_size = chunk_cap;
    arena->total_allocated    = chunk_cap;
    arena->alloc              = alloc;
    return arena;
}

void cobalt_arena_destroy(cobalt_arena_t *arena)
{
    if (!arena) {
        return;
    }
    arena_chunk_t *curr = arena->chunks;
    while (curr) {
        arena_chunk_t *next = curr->next;
        arena->alloc->free(arena->alloc, curr);
        curr = next;
    }
    arena->alloc->free(arena->alloc, arena);
}

void *cobalt_arena_alloc(cobalt_arena_t *arena, size_t size)
{
    if (!arena || size == 0) {
        return NULL;
    }

    size_t aligned_size = (size + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);

    arena_chunk_t *chunk = arena->current;
    if (chunk && (chunk->capacity - chunk->used >= aligned_size)) {
        void *ptr = chunk->data + chunk->used;
        chunk->used += aligned_size;
        return ptr;
    }

    /* Space exhausted in current chunk: allocate a new chunk without touching previous chunks */
    size_t new_cap = aligned_size > arena->default_chunk_size ? aligned_size : arena->default_chunk_size;
    arena_chunk_t *new_chunk = (arena_chunk_t *)arena->alloc->alloc(
        arena->alloc, sizeof(arena_chunk_t) + new_cap);
    if (!new_chunk) {
        return NULL;
    }

    new_chunk->next     = arena->chunks;
    new_chunk->capacity = new_cap;
    new_chunk->used     = aligned_size;
    arena->chunks       = new_chunk;
    arena->current      = new_chunk;
    arena->total_allocated += new_cap;

    return new_chunk->data;
}

void cobalt_arena_reset(cobalt_arena_t *arena)
{
    if (!arena) {
        return;
    }
    arena_chunk_t *curr = arena->chunks;
    while (curr) {
        curr->used = 0;
        curr = curr->next;
    }
    arena->current = arena->chunks;
}
```

- [ ] **Step 4: Run tests and verify zero regressions**

Run: `cmake --build build --target cobalt_test && ctest --test-dir build -R test_arena --output-on-failure`
Expected: 100% tests passed.

- [ ] **Step 5: Commit**

```bash
git add src/memory/arena.c tests/unit/test_arena.c
git commit -m "refactor(memory): ♻️ implement chunked block allocation in arena for pointer stability"
```

---

## Task 2: HashMap & Set Robin Hood Open Addressing

**Files:**
- Modify: `src/container/hashmap.c`
- Modify: `tests/unit/test_hashmap.c`

- [ ] **Step 1: Write failing/boundary tests in test_hashmap.c**

Add tests in `tests/unit/test_hashmap.c`:
- `test_hashmap_collision_and_backward_shift`: Insert keys with identical/near hash codes, remove middle keys, assert that all remaining keys are still found.
- `test_hashmap_high_load_factor_rehash`: Insert 5,000 keys, verify values, remove 2,500 keys, verify table consistency and absence of memory leaks.

- [ ] **Step 2: Run test to verify initial state**

Run: `ctest --test-dir build -R test_hashmap --output-on-failure`

- [ ] **Step 3: Implement Robin Hood Flat-Table in src/container/hashmap.c**

1. Replace `hashmap_node_t` linked-list structure with:
```c
#define COBALT_HASHMAP_EMPTY_PSL ((uint32_t)0xFFFFFFFF)

typedef struct {
    void        *key;
    void        *value;
    uint32_t     hash;
    uint32_t     psl;
    size_t       key_len;
    uint8_t      key_owned;
} hashmap_slot_t;

typedef struct {
    hashmap_slot_t     *slots;
    size_t              capacity;
    size_t              mask;
    size_t              size;
    size_t              max_load;
    cobalt_hash_func_t  hash_func;
    cobalt_equal_func_t equal_func;
    cobalt_allocator_t *alloc;
} hashmap_impl_t;
```
2. Implement slot allocation and power-of-two capacity sizing (default initial capacity 16).
3. Implement `lookup_slot` with Early Exit:
   - When probing `idx`, if `slots[idx].psl == COBALT_HASHMAP_EMPTY_PSL || slots[idx].psl < cur_psl`, key is guaranteed not found.
4. Implement `put` with Robin Hood swap:
   - On key match, update value and return 0.
   - On new insertion, when encountering a slot with `slots[idx].psl < cur_entry.psl`, swap `cur_entry` and `slots[idx]`, continue probing with displaced entry.
   - Resize when `impl.size >= impl.max_load` (0.75 capacity).
5. Implement `remove` with Backward Shift:
   - Free owned key if `key_owned != 0`.
   - Shift backward elements that have `psl > 0`, decrementing their PSL by 1.
   - Mark the final vacated slot as `EMPTY_PSL`.
6. Implement `cobalt_hashmap_iterator` iterating sequentially through `slots` array skipping empty slots.

- [ ] **Step 4: Run tests and verify all hashmap, map, and set tests pass**

Run: `ctest --test-dir build -R "test_hashmap|test_set|test_map|bench_hashmap" --output-on-failure`
Expected: 100% tests passed.

- [ ] **Step 5: Commit**

```bash
git add src/container/hashmap.c tests/unit/test_hashmap.c
git commit -m "refactor(container): ♻️ convert hashmap to robin hood open addressing with backward shift"
```

---

## Task 3: Bounded MPMC Lock-Free Queue (Vyukov)

**Files:**
- Create: `include/cobalt/container/lfqueue.h`
- Create: `src/container/lfqueue.c`
- Create: `tests/unit/test_lfqueue.c`
- Modify: `include/cobalt/cobalt.h`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create header include/cobalt/container/lfqueue.h**

```c
#ifndef COBALT_CONTAINER_LFQUEUE_H
#define COBALT_CONTAINER_LFQUEUE_H

/**
 * @file lfqueue.h
 * @brief Lock-free bounded Multi-Producer Multi-Consumer (MPMC) queue
 * @details Implemented using Dmitry Vyukov's bounded queue algorithm with C11 atomics.
 *          Features 64-byte cache-line aligned producer and consumer cursors to eliminate
 *          false sharing across cores.
 */

#include "cobalt/memory/allocator.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cobalt_lfqueue cobalt_lfqueue_t;

cobalt_lfqueue_t *cobalt_lfqueue_create(size_t capacity);
cobalt_lfqueue_t *cobalt_lfqueue_create_with_allocator(size_t capacity, cobalt_allocator_t *alloc);
void              cobalt_lfqueue_destroy(cobalt_lfqueue_t *queue);

int               cobalt_lfqueue_enqueue(cobalt_lfqueue_t *queue, void *data);
int               cobalt_lfqueue_dequeue(cobalt_lfqueue_t *queue, void **out_data);

size_t            cobalt_lfqueue_size(const cobalt_lfqueue_t *queue);
int               cobalt_lfqueue_is_empty(const cobalt_lfqueue_t *queue);
int               cobalt_lfqueue_is_full(const cobalt_lfqueue_t *queue);
size_t            cobalt_lfqueue_capacity(const cobalt_lfqueue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* COBALT_CONTAINER_LFQUEUE_H */
```

- [ ] **Step 2: Register in CMakeLists.txt and include/cobalt/cobalt.h**

Add `src/container/lfqueue.c` to `COBALT_SOURCES` in `CMakeLists.txt`.
Add `#include "cobalt/container/lfqueue.h"` to `include/cobalt/cobalt.h`.

- [ ] **Step 3: Create tests/unit/test_lfqueue.c**

```c
#include "test_framework.h"
#include "cobalt/container/lfqueue.h"
#include "cobalt/platform/thread.h"
#include <stdint.h>
#include <stdlib.h>

TEST(lfqueue, create_destroy) {
    cobalt_lfqueue_t *q = cobalt_lfqueue_create(10);
    TEST_ASSERT(q != NULL);
    TEST_ASSERT(cobalt_lfqueue_capacity(q) == 16); /* rounded to next pow2 */
    TEST_ASSERT(cobalt_lfqueue_is_empty(q));
    TEST_ASSERT(!cobalt_lfqueue_is_full(q));
    TEST_ASSERT(cobalt_lfqueue_size(q) == 0);
    cobalt_lfqueue_destroy(q);
}

TEST(lfqueue, single_thread_fifo) {
    cobalt_lfqueue_t *q = cobalt_lfqueue_create(4);
    TEST_ASSERT(q != NULL);

    uintptr_t val1 = 100, val2 = 200, val3 = 300, val4 = 400;
    TEST_ASSERT(cobalt_lfqueue_enqueue(q, (void *)val1) == 0);
    TEST_ASSERT(cobalt_lfqueue_enqueue(q, (void *)val2) == 0);
    TEST_ASSERT(cobalt_lfqueue_enqueue(q, (void *)val3) == 0);
    TEST_ASSERT(cobalt_lfqueue_enqueue(q, (void *)val4) == 0);
    /* Full */
    TEST_ASSERT(cobalt_lfqueue_is_full(q));
    TEST_ASSERT(cobalt_lfqueue_enqueue(q, (void *)500) == -1);

    void *out = NULL;
    TEST_ASSERT(cobalt_lfqueue_dequeue(q, &out) == 0 && (uintptr_t)out == 100);
    TEST_ASSERT(cobalt_lfqueue_dequeue(q, &out) == 0 && (uintptr_t)out == 200);
    TEST_ASSERT(cobalt_lfqueue_dequeue(q, &out) == 0 && (uintptr_t)out == 300);
    TEST_ASSERT(cobalt_lfqueue_dequeue(q, &out) == 0 && (uintptr_t)out == 400);
    /* Empty */
    TEST_ASSERT(cobalt_lfqueue_is_empty(q));
    TEST_ASSERT(cobalt_lfqueue_dequeue(q, &out) == -1);

    cobalt_lfqueue_destroy(q);
}

#define LF_CONCURRENT_ITEMS 50000
#define LF_NUM_PRODUCERS 4
#define LF_NUM_CONSUMERS 4

typedef struct {
    cobalt_lfqueue_t *q;
    int               thread_id;
} lf_worker_arg_t;

static void *lf_producer_worker(void *arg) {
    lf_worker_arg_t *w = (lf_worker_arg_t *)arg;
    for (int i = 0; i < LF_CONCURRENT_ITEMS; i++) {
        uintptr_t val = (uintptr_t)(w->thread_id * LF_CONCURRENT_ITEMS + i + 1);
        while (cobalt_lfqueue_enqueue(w->q, (void *)val) != 0) {
            cobalt_thread_yield();
        }
    }
    return NULL;
}

static uint64_t g_consumer_sums[LF_NUM_CONSUMERS] = {0};

static void *lf_consumer_worker(void *arg) {
    lf_worker_arg_t *w = (lf_worker_arg_t *)arg;
    uint64_t sum = 0;
    int items_popped = 0;
    int target_total = (LF_NUM_PRODUCERS * LF_CONCURRENT_ITEMS) / LF_NUM_CONSUMERS;

    while (items_popped < target_total) {
        void *data = NULL;
        if (cobalt_lfqueue_dequeue(w->q, &data) == 0) {
            sum += (uintptr_t)data;
            items_popped++;
        } else {
            cobalt_thread_yield();
        }
    }
    g_consumer_sums[w->thread_id] = sum;
    return NULL;
}

TEST(lfqueue, multi_threaded_concurrency) {
    cobalt_lfqueue_t *q = cobalt_lfqueue_create(1024);
    TEST_ASSERT(q != NULL);

    cobalt_thread_t producers[LF_NUM_PRODUCERS];
    cobalt_thread_t consumers[LF_NUM_CONSUMERS];
    lf_worker_arg_t prod_args[LF_NUM_PRODUCERS];
    lf_worker_arg_t cons_args[LF_NUM_CONSUMERS];

    for (int i = 0; i < LF_NUM_CONSUMERS; i++) {
        cons_args[i].q = q;
        cons_args[i].thread_id = i;
        g_consumer_sums[i] = 0;
        TEST_ASSERT(cobalt_thread_create(&consumers[i], lf_consumer_worker, &cons_args[i]) == 0);
    }

    for (int i = 0; i < LF_NUM_PRODUCERS; i++) {
        prod_args[i].q = q;
        prod_args[i].thread_id = i;
        TEST_ASSERT(cobalt_thread_create(&producers[i], lf_producer_worker, &prod_args[i]) == 0);
    }

    for (int i = 0; i < LF_NUM_PRODUCERS; i++) {
        cobalt_thread_join(producers[i]);
    }
    for (int i = 0; i < LF_NUM_CONSUMERS; i++) {
        cobalt_thread_join(consumers[i]);
    }

    uint64_t total_sum = 0;
    for (int i = 0; i < LF_NUM_CONSUMERS; i++) {
        total_sum += g_consumer_sums[i];
    }

    /* Expected sum of all produced values */
    uint64_t expected_sum = 0;
    for (int p = 0; p < LF_NUM_PRODUCERS; p++) {
        for (int i = 0; i < LF_CONCURRENT_ITEMS; i++) {
            expected_sum += (uint64_t)(p * LF_CONCURRENT_ITEMS + i + 1);
        }
    }
    TEST_ASSERT(total_sum == expected_sum);
    TEST_ASSERT(cobalt_lfqueue_is_empty(q));

    cobalt_lfqueue_destroy(q);
}
```

- [ ] **Step 4: Register test in tests/CMakeLists.txt**

Add `add_unit_test(test_lfqueue test_lfqueue.c)` in `tests/CMakeLists.txt`.

- [ ] **Step 5: Implement src/container/lfqueue.c**

Implement `src/container/lfqueue.c` following Dmitry Vyukov's MPMC algorithm with:
- `_Alignas(64)` on `tail`, `head`, and read-mostly members.
- Power-of-two capacity rounding.
- Safe allocation via `cobalt_allocator_t`.
- Strict memory ordering (`acquire` on sequence read, `release` on sequence commit).

- [ ] **Step 6: Run test suite**

Run: `cmake --build build -j4 && ctest --test-dir build -R test_lfqueue --output-on-failure`
Expected: 100% tests passed.

- [ ] **Step 7: Commit**

```bash
git add include/cobalt/container/lfqueue.h src/container/lfqueue.c tests/unit/test_lfqueue.c CMakeLists.txt tests/CMakeLists.txt include/cobalt/cobalt.h
git commit -m "feat(container): ✨ implement bounded lock-free mpmc queue (vyukov)"
```

---

## Task 4: Benchmarking Suite Expansion & Performance Quantification

**Files:**
- Create: `tests/benchmark/benchmark_lfqueue.c`
- Modify: `tests/benchmark/bench_runner.c`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create tests/benchmark/benchmark_lfqueue.c**

Implement benchmark comparing `cobalt_lfqueue_t` vs `cobalt_tsqueue_t`:
1. Single-threaded 200,000 enq/deq operations ops/sec.
2. Multi-threaded 8-thread contention ops/sec.
3. Print speedup ratio.

- [ ] **Step 2: Register in tests/CMakeLists.txt and bench_runner.c**

Add executable `benchmark_lfqueue` in `tests/CMakeLists.txt`.
Wire `bench_lfqueue` into `tests/benchmark/bench_runner.c`.

- [ ] **Step 3: Run benchmark runner**

Run: `cmake --build build -j4 && ./build/bench_runner`
Verify all benchmarks (vector, hashmap, treemap, set, deque, tsqueue, lfqueue) run and output statistics.

- [ ] **Step 4: Commit**

```bash
git add tests/benchmark/benchmark_lfqueue.c tests/benchmark/bench_runner.c tests/CMakeLists.txt
git commit -m "test(bench): ⚡ add lfqueue benchmark and wire into bench_runner"
```

---

## Task 5: Static Analysis, Formatting & Full Regression Verification

**Files:**
- All touched files

- [ ] **Step 1: Run clang-format on all modified and new files**

Run:
```bash
clang-format -i src/memory/arena.c src/container/hashmap.c src/container/lfqueue.c include/cobalt/container/lfqueue.h include/cobalt/cobalt.h tests/unit/test_arena.c tests/unit/test_hashmap.c tests/unit/test_lfqueue.c tests/benchmark/benchmark_lfqueue.c tests/benchmark/bench_runner.c
```

- [ ] **Step 2: Run clang-tidy check**

Run:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCOBALT_RUN_TIDY=ON
cmake --build build -j4
```
Expected: 0 warnings, 0 errors.

- [ ] **Step 3: Run full CTest suite**

Run:
```bash
ctest --test-dir build --output-on-failure
```
Expected: 42/42 (or 43/43) tests passed (100%).

- [ ] **Step 4: Commit and finalize**

```bash
git add -u
git commit -m "chore(quality): 🎨 format and lint check clean"
```
