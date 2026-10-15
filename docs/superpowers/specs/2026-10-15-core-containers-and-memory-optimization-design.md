# Cobalt Core Containers & Memory Optimization — Specification

**Date:** 2026-10-15  
**Status:** Approved  
**Scope:** Core Collections (Layer 4), Memory Subsystem (Layer 7a), Concurrency & Benchmarking  
**Authors:** Antigravity & Cobalt Maintainer

---

## 1. Overview & Goals

This specification details the comprehensive optimization and hardening of Cobalt's core containers and memory allocators. It addresses critical performance bottlenecks, memory layout efficiency, cache locality, and memory safety without breaking backward compatibility:

1. **HashMap & Set (Robin Hood Open Addressing)**: Replace node-per-entry separate chaining with a contiguous flat array using Robin Hood hashing with backward-shift deletion, eliminating per-element `malloc` overhead and drastically improving CPU cache locality.
2. **Arena Allocator (Chunked Block Architecture)**: Replace single-buffer `realloc` growth with a non-moving chunked linked-list architecture, eliminating dangling pointer vulnerabilities when expanding while preserving $O(1)$ linear bump allocation and instant bulk reset.
3. **Lock-Free Concurrent Queue (Vyukov Bounded MPMC)**: Introduce `cobalt_lfqueue_t`, an atomic, cache-line-padded, lock-free bounded ring queue based on Dmitry Vyukov's algorithm for high-throughput multi-core workloads.
4. **Validation & Benchmarking**: Expand unit tests with pointer stability, Robin Hood collision/deletion tests, and multi-threaded stress suites; integrate benchmark comparisons into `bench_runner`.

---

## 2. Design Section 1 — HashMap & Set: Robin Hood Open Addressing

### 2.1 Problem Analysis
In the current implementation (`src/container/hashmap.c`):
- Each key-value entry requires allocating a separate `hashmap_node_t` via `alloc->alloc()` / `malloc()`.
- Collision resolution uses separate chaining (`hashmap_node_t **buckets`), leading to pointer chasing through scattered heap memory on lookup and insertion.
- The `cobalt_set_t` container is directly implemented on top of `cobalt_hashmap_t` and inherits all chaining overheads.

### 2.2 Data Structure & Memory Layout
All entries are stored in a single, contiguous array of slots sized to a power of two. Each slot contains the full entry metadata and a Probe Sequence Length (PSL):

```c
#define COBALT_HASHMAP_EMPTY_PSL ((uint32_t)0xFFFFFFFF)

typedef struct {
    void        *key;       /* Key pointer (heap copy owned by map if key_owned != 0) */
    void        *value;     /* Stored value pointer */
    uint32_t     hash;      /* Cached 32-bit hash digest for rapid filtering */
    uint32_t     psl;       /* Probe sequence length (0 = initial bucket, EMPTY_PSL = unused) */
    size_t       key_len;   /* Length of key in bytes */
    uint8_t      key_owned; /* 1 if key buffer is owned and must be freed on removal */
} hashmap_slot_t;

typedef struct {
    hashmap_slot_t     *slots;        /* Contiguous array of capacity elements */
    size_t              capacity;     /* Number of slots (always power-of-two, min 16) */
    size_t              mask;         /* capacity - 1 */
    size_t              size;         /* Count of active elements */
    size_t              max_load;     /* capacity * 0.75 threshold triggering rehash */
    cobalt_hash_func_t  hash_func;    /* Custom or default (FNV-1a) hash function */
    cobalt_equal_func_t equal_func;   /* Custom or default (strcmp / memcmp) equal function */
    cobalt_allocator_t *alloc;        /* Memory allocator */
} hashmap_impl_t;

struct cobalt_hashmap {
    cobalt_map_t   base;  /* Polymorphic map interface */
    hashmap_impl_t impl;
};
```

### 2.3 Algorithms

#### 2.3.1 Lookup (`cobalt_hashmap_get` / `cobalt_hashmap_get_ext`)
1. Compute `hash = map->impl.hash_func ? map->impl.hash_func(key, key_len) : hash_string(key)`.
2. Compute starting index: `idx = hash & map->impl.mask`, initialize `cur_psl = 0`.
3. Loop:
   - If `slots[idx].psl == COBALT_HASHMAP_EMPTY_PSL`: return `NULL` immediately.
   - If `slots[idx].psl < cur_psl`: by Robin Hood invariant, any key with equal or greater PSL would have been placed here or earlier. Therefore, the key does not exist; return `NULL` (Early Exit).
   - If `slots[idx].hash == hash` and `key_equals(slots[idx], key, key_len)`: return `slots[idx].value`.
   - Advance: `idx = (idx + 1) & mask`, `cur_psl++`.

#### 2.3.2 Insertion (`cobalt_hashmap_put` / `cobalt_hashmap_put_ext`)
1. If `impl.size >= impl.max_load` or `impl.capacity == 0`, resize to `max(16, capacity * 2)`:
   - Allocate new slot array, re-insert all non-empty items with PSL reset to 0, free old slot array.
2. Search for existing matching key using lookup loop:
   - If found: update `slots[match_idx].value = value` and return 0.
3. If new key:
   - Construct `curr_entry = {key, value, hash, psl = 0, key_len, key_owned}`.
   - Begin probe at `idx = hash & mask`:
     - If `slots[idx].psl == COBALT_HASHMAP_EMPTY_PSL`: place `curr_entry` into `slots[idx]`, `impl.size++`, return 0.
     - If `slots[idx].psl < curr_entry.psl`: swap `curr_entry` and `slots[idx]` (Robin Hood swap). The displaced entry becomes the new `curr_entry`.
     - Advance: `idx = (idx + 1) & mask`, `curr_entry.psl++`.

#### 2.3.3 Deletion (`cobalt_hashmap_remove` / `cobalt_hashmap_remove_ext`)
1. Locate key slot `idx`. If not found, return -1.
2. If `slots[idx].key_owned && slots[idx].key`: free the key memory.
3. Backward Shift (Tombstone-free compaction):
   - Set `curr = idx`.
   - Loop:
     - `next = (curr + 1) & mask`.
     - If `slots[next].psl == COBALT_HASHMAP_EMPTY_PSL || slots[next].psl == 0`: break.
     - Copy `slots[next]` to `slots[curr]`, decrementing `slots[curr].psl--`.
     - `curr = next`.
   - Set `slots[curr].psl = COBALT_HASHMAP_EMPTY_PSL`, clear pointers, `impl.size--`. Return 0.

#### 2.3.4 Iterators & Set Compatibility
- `cobalt_hashmap_iterator`: Scans sequentially through `impl.slots` skipping `EMPTY_PSL` slots.
- `cobalt_set_t`: Uses `cobalt_hashmap_t` without modification. All performance benefits automatically propagate to Set operations.

---

## 3. Design Section 2 — Arena Allocator: Chunked Block Architecture

### 3.1 Problem Analysis
In `src/memory/arena.c`:
- `cobalt_arena_alloc()` performs `realloc()` on its single internal buffer when capacity is exceeded.
- If `realloc()` moves the buffer to a new memory address, **every previously allocated pointer becomes dangling**.
- This violates standard Arena semantics where returned pointers must remain valid until the arena is reset or destroyed.

### 3.2 Data Structure
The arena manages a singly-linked list of immutable chunks:

```c
typedef struct arena_chunk {
    struct arena_chunk *next;      /* Next chunk in the allocated list */
    size_t              capacity;  /* Byte capacity of data buffer */
    size_t              used;      /* Bytes currently sub-allocated */
    _Alignas(max_align_t) unsigned char data[]; /* Aligned buffer memory */
} arena_chunk_t;

struct cobalt_arena {
    arena_chunk_t      *current;            /* Active chunk for linear bump allocations */
    arena_chunk_t      *chunks;             /* Head of all allocated chunks */
    size_t              default_chunk_size; /* Default chunk capacity (at least 4096 bytes) */
    size_t              total_allocated;    /* Cumulative capacity across all chunks */
    cobalt_allocator_t *alloc;              /* Backing allocator */
};
```

### 3.3 Lifecycle & Operations

#### 3.3.1 Creation (`cobalt_arena_create` / `cobalt_arena_create_with_allocator`)
- Sizing: `default_chunk_size = initial_size > 0 ? initial_size : 4096`.
- Allocate arena control header.
- Allocate initial chunk with capacity `default_chunk_size`, link as `chunks` and `current`.

#### 3.3.2 Allocation (`cobalt_arena_alloc`)
1. Calculate aligned size:
   `aligned_size = (size + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);`
2. Fast Path:
   - If `current && (current->capacity - current->used >= aligned_size)`:
     - `ptr = current->data + current->used;`
     - `current->used += aligned_size;`
     - Return `ptr`.
3. Slow Path (Allocate New Chunk):
   - Determine chunk capacity: `chunk_cap = aligned_size > arena->default_chunk_size ? aligned_size : arena->default_chunk_size;`
   - Allocate `sizeof(arena_chunk_t) + chunk_cap` via `arena->alloc->alloc()`.
   - Initialize new chunk (`capacity = chunk_cap, used = aligned_size, next = NULL`).
   - Link new chunk into `arena->chunks` chain and set `arena->current = new_chunk`.
   - Update `arena->total_allocated += chunk_cap`.
   - Return `new_chunk->data`.
4. **Invariant**: Previously allocated chunks are never relocated; all existing pointers remain 100% stable.

#### 3.3.3 Reset (`cobalt_arena_reset`)
- Do not free chunks back to the OS.
- Iterate over all chunks in `arena->chunks`, setting `chunk->used = 0`.
- Reset `arena->current = arena->chunks`.
- All memory is immediately reusable in $O(K)$ time (where $K$ is chunk count, typically < 10) with zero system calls.

#### 3.3.4 Destruction (`cobalt_arena_destroy`)
- Walk chunk list and free each chunk via `arena->alloc->free()`.
- Free arena header.

---

## 4. Design Section 3 — Lock-Free Concurrent Queue: Vyukov Bounded MPMC

### 4.1 Problem Analysis
`cobalt_tsqueue` protects a node-allocated linked list with a `cobalt_mutex_t`. Under multi-threaded contention (e.g. 8 threads), benchmark runs show high latency due to OS mutex futex sleeps, context switches, and cache line contention on shared locks.

### 4.2 Architecture & False Sharing Prevention
`cobalt_lfqueue_t` is a bounded Multi-Producer Multi-Consumer (MPMC) lock-free ring buffer based on Dmitry Vyukov's algorithm.

To eliminate false sharing across CPU cores:
- Producer head and consumer tail are isolated on dedicated 64-byte cache lines.
- Constants and buffer pointers are isolated on separate cache lines.

```c
#define COBALT_CACHE_LINE_SIZE 64

typedef struct {
    atomic_size_t sequence; /* Slot state generation number */
    void         *data;     /* Payload pointer */
} cobalt_lfqueue_cell_t;

struct cobalt_lfqueue {
    /* Producer cache line */
    _Alignas(COBALT_CACHE_LINE_SIZE) atomic_size_t tail;

    /* Consumer cache line */
    _Alignas(COBALT_CACHE_LINE_SIZE) atomic_size_t head;

    /* Read-mostly configuration cache line */
    _Alignas(COBALT_CACHE_LINE_SIZE) size_t capacity;
    size_t                 mask;    /* capacity - 1 */
    cobalt_allocator_t    *alloc;
    cobalt_lfqueue_cell_t *buffer;
};
```

### 4.3 Algorithm Implementation

#### 4.3.1 Initialization
- Round requested capacity up to the next power of two (minimum 2).
- Allocate buffer of `capacity` cells.
- For each cell $i \in [0, capacity - 1]$, store `cell[i].sequence = i`.
- Initialize `head = 0`, `tail = 0`.

#### 4.3.2 Enqueue (`cobalt_lfqueue_enqueue`)
```c
int cobalt_lfqueue_enqueue(cobalt_lfqueue_t *queue, void *data) {
    if (!queue) return -1;
    cobalt_lfqueue_cell_t *cell;
    size_t pos = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    for (;;) {
        cell = &queue->buffer[pos & queue->mask];
        size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;
        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&queue->tail, &pos, pos + 1,
                                                      memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            return -1; /* Queue is full */
        } else {
            pos = atomic_load_explicit(&queue->tail, memory_order_relaxed);
        }
    }
    cell->data = data;
    atomic_store_explicit(&cell->sequence, pos + 1, memory_order_release);
    return 0;
}
```

#### 4.3.3 Dequeue (`cobalt_lfqueue_dequeue`)
```c
int cobalt_lfqueue_dequeue(cobalt_lfqueue_t *queue, void **out_data) {
    if (!queue || !out_data) return -1;
    cobalt_lfqueue_cell_t *cell;
    size_t pos = atomic_load_explicit(&queue->head, memory_order_relaxed);
    for (;;) {
        cell = &queue->buffer[pos & queue->mask];
        size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&queue->head, &pos, pos + 1,
                                                      memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            return -1; /* Queue is empty */
        } else {
            pos = atomic_load_explicit(&queue->head, memory_order_relaxed);
        }
    }
    *out_data = cell->data;
    atomic_store_explicit(&cell->sequence, pos + queue->mask + 1, memory_order_release);
    return 0;
}
```

#### 4.3.4 Public API
- `cobalt_lfqueue_t *cobalt_lfqueue_create(size_t capacity);`
- `cobalt_lfqueue_t *cobalt_lfqueue_create_with_allocator(size_t capacity, cobalt_allocator_t *alloc);`
- `void              cobalt_lfqueue_destroy(cobalt_lfqueue_t *queue);`
- `int               cobalt_lfqueue_enqueue(cobalt_lfqueue_t *queue, void *data);`
- `int               cobalt_lfqueue_dequeue(cobalt_lfqueue_t *queue, void **out_data);`
- `size_t            cobalt_lfqueue_size(const cobalt_lfqueue_t *queue);`
- `int               cobalt_lfqueue_is_empty(const cobalt_lfqueue_t *queue);`
- `int               cobalt_lfqueue_is_full(const cobalt_lfqueue_t *queue);`
- `size_t            cobalt_lfqueue_capacity(const cobalt_lfqueue_t *queue);`

---

## 5. Verification & Testing Plan

### 5.1 Unit Tests
1. **Existing Baseline**:
   - Run full test suite; ensure all 41 existing test targets pass with zero regressions.
2. **`tests/unit/test_arena.c` Enhancements**:
   - `test_arena_pointer_stability`: Allocate 100 items of 256 bytes across an initial 1024-byte arena. Record pointer addresses and fill each with unique patterns. Verify that after multiple chunk allocations, all 100 original pointer addresses remain valid and uncorrupted.
   - `test_arena_reset_reuse`: Verify that `cobalt_arena_reset` enables immediate sub-allocation with zero new heap chunk allocations.
3. **`tests/unit/test_hashmap.c` Enhancements**:
   - `test_hashmap_backward_shift`: Insert sequential keys causing controlled collisions, delete intermediate keys, and verify subsequent lookups succeed for all remaining keys.
   - `test_hashmap_large_scale`: Insert 10,000 keys, verify 100% lookup accuracy, delete 5,000 keys, verify consistency.
4. **`tests/unit/test_lfqueue.c` (New Target)**:
   - Creation with non-power-of-two (properly rounded), destruction of empty and partially-filled queues.
   - Single-thread boundary tests (wrap-around, full return -1, empty return -1).
   - Multi-threaded stress test: 4 producer threads + 4 consumer threads pushing and popping 200,000 items, verifying total sum and conservation of all elements with no deadlocks.
   - Custom allocator injection test.

### 5.2 Performance Benchmarking
1. **`benchmark_hashmap` & `benchmark_set`**:
   - Measure 100,000-op throughput (insert, find hit, find miss, delete) comparing old vs new.
2. **`tests/benchmark/benchmark_lfqueue.c` (New Target)**:
   - Single-thread enqueue/dequeue throughput.
   - 8-thread multi-threaded contention throughput compared against `cobalt_tsqueue`.
3. **`bench_runner`**:
   - Integrate `benchmark_lfqueue` into the automated runner.

### 5.3 Quality Gates
- **Compiler flags**: Strict `-Wall -Wextra -Werror` clean compilation.
- **Formatting**: `clang-format -i` compliant.
- **Static analysis**: Clean run of `clang-tidy`.
- **Sanitizers**: ASan, UBSan, and Valgrind clean (zero leaks, zero buffer errors).
