# SPEC: Lock-Free SkipList with Epoch-Based Reclamation (EBR) Engine

**Date:** 2026-10-17  
**Scope:** Layer 7 Memory (`cobalt_ebr_t`) & Layer 4 Container (`cobalt_lfskiplist_t`)  
**Status:** Approved — Ready for Implementation Planning  
**Target Version:** Cobalt v2.7.0  

---

## 1. Executive Summary

As multi-core architectures scale, traditional mutex-protected associative containers (e.g. `pthread_mutex_t` around `cobalt_treemap_t`) suffer from severe cache line bouncing, lock contention, and serialization bottlenecks. While Cobalt v2.5.0 introduced the bounded MPMC lock-free queue (`cobalt_lfqueue_t`), it relies on fixed-capacity arrays without dynamic node reclamation.

This specification defines a production-grade, pure C11, zero-dependency concurrent data structure suite:
1. **Layer 7 (Memory Layer) — `cobalt_ebr_t` (`include/cobalt/memory/ebr.h`)**:
   A general-purpose 3-Epoch Safe Memory Reclamation (SMR) engine that enables reader threads to traverse dynamic concurrent nodes without locking while guaranteeing mathematical immunity against Use-After-Free (UAF) and ABA memory hazards.
2. **Layer 4 (Container Layer) — `cobalt_lfskiplist_t` (`include/cobalt/container/lfskiplist.h`)**:
   A pure Lock-Free Concurrent SkipList implementing the Fraser / Herlihy-Lev-Shavit (HLS) algorithm with marked pointers, wait-free point reads, lock-free inserts/deletes, and thread-safe ordered range scan iterators.

---

## 2. Architecture & Layering

Following Cobalt's strict upward-only architectural dependency rules:

```
┌─────────────────────────────────────────────────────────────┐
│                 Multi-Threaded Applications                 │
│         (Concurrent reads, writes, upserts, range scans)    │
└──────────────────────────────┬──────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────┐
│  Layer 4 (Container): cobalt_lfskiplist_t                   │
│  File: include/cobalt/container/lfskiplist.h                │
│  Implementation: src/container/lfskiplist.c                 │
│  - 32-level geometric height distribution (P = 0.5)         │
│  - Marked pointer bit 0 for two-phase deletion              │
│  - Cooperative physical unlinking on traversal              │
│  - Generic void *key, void *value with compare_func_t       │
│  - Range iterator with pinned EBR lifecycle                 │
└──────────────────────────────┬──────────────────────────────┘
                               │ Upward-only dependency
┌──────────────────────────────▼──────────────────────────────┐
│  Layer 7 (Memory): cobalt_ebr_t                             │
│  File: include/cobalt/memory/ebr.h                          │
│  Implementation: src/memory/ebr.c                           │
│  - 3-Epoch cyclic rotation (Epoch 0, 1, 2)                  │
│  - Per-thread 64-byte aligned handles (0 false sharing)     │
│  - Per-thread retired batches (64 items) with batch advance │
│  - POSIX thread exit cleanup callback via pthread_key       │
│  - Full cobalt_allocator_t integration                      │
└──────────────────────────────┬──────────────────────────────┘
                               │ Upward-only dependency
┌──────────────────────────────▼──────────────────────────────┐
│  Layer 8 (Platform): atomic.h & thread.h                    │
│  File: include/cobalt/platform/atomic.h                     │
│  - C11 stdatomic.h, memory orders (acquire, release, seq_cst│
│  - Cacheline alignment (_Alignas(64))                       │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Layer 7: Epoch-Based Reclamation (`cobalt_ebr_t`)

### 3.1 Mathematical Foundation
Dynamic lock-free data structures cannot invoke `free()` immediately when unlinking a node because concurrent reader threads may already have dereferenced the node's pointer and loaded it into CPU registers or stack frames.

The 3-Epoch reclamation theorem states:
- Global time is divided into monotonically advancing epochs: $e \in \{0, 1, 2\}$ via $(e + 1) \pmod 3$.
- A thread enters an epoch critical section by publishing its active epoch $a_i = (e_{\text{global}} \ll 1) \mid 1$.
- Any node retired during epoch $e$ can only be observed by threads whose active epoch $a_i$ corresponds to epoch $e$ (or older).
- When the system determines that **all currently active threads** have observed $e_{\text{global}} \ge e + 1$, no active thread can hold a reference to nodes retired in epoch $e$.
- Therefore, when $e_{\text{global}}$ reaches $(e + 2) \pmod 3$, all nodes retired in epoch $e$ are guaranteed safe to be physically deallocated via `free()`.

### 3.2 Data Structures

```c
typedef struct cobalt_ebr_node {
    void                    *ptr;
    void                   (*free_fn)(void *ptr, void *user_data);
    void                    *user_data;
    struct cobalt_ebr_node  *next;
} cobalt_ebr_node_t;

typedef struct cobalt_ebr_handle {
    _Alignas(64) atomic_uint_fast64_t active_epoch; // Bit 0: active flag; Bit 1..63: epoch
    cobalt_ebr_node_t                *local_retired[3]; // Local retired lists for e = 0, 1, 2
    size_t                            retired_count;     // Batched counter
    _Atomic(struct cobalt_ebr_handle *) next;            // Registry linked list
    struct cobalt_ebr                *owner;             // Owner EBR instance
    atomic_bool                       registered;        // Active registration flag
} cobalt_ebr_handle_t;

typedef struct cobalt_ebr {
    _Alignas(64) atomic_uint_fast64_t global_epoch;      // Monotonic epoch counter
    _Alignas(64) atomic_uintptr_t     head_handle;       // Lock-free list of thread handles
    cobalt_allocator_t               *allocator;         // Memory allocator
    atomic_size_t                     total_handles;     // Total registered threads
} cobalt_ebr_t;
```

### 3.3 Execution Protocols
1. **Critical Section Entry (`cobalt_ebr_enter`)**:
   ```c
   uint64_t g = atomic_load_explicit(&ebr->global_epoch, memory_order_acquire);
   atomic_store_explicit(&handle->active_epoch, (g << 1) | 1, memory_order_seq_cst);
   ```
2. **Critical Section Exit (`cobalt_ebr_exit`)**:
   ```c
   atomic_store_explicit(&handle->active_epoch, 0, memory_order_release);
   ```
3. **Retire Object (`cobalt_ebr_retire`)**:
   - Pushes node to `handle->local_retired[g % 3]`.
   - Increments `retired_count`.
   - If `retired_count >= 64`, invokes `cobalt_ebr_try_advance(ebr)`.
4. **Epoch Advance & Bulk Reclamation (`cobalt_ebr_try_advance`)**:
   - Inspects `active_epoch` of all registered handles in `head_handle`.
   - If every active handle has `(active_epoch >> 1) == g` (none lagging in $g - 1$), successfully increments `global_epoch` via CAS.
   - Upon advancing to $g + 1$, safely reclaims and frees all nodes in `local_retired[(g + 2) % 3]`.
5. **Thread Exit Safety**:
   - Uses a `pthread_key_t` destructor to auto-unregister handles and transfer leftover retired nodes to an orphaned list in `ebr`, preventing stalled epochs if a thread terminates abruptly.

---

## 4. Layer 4: Lock-Free SkipList (`cobalt_lfskiplist_t`)

### 4.1 Node Layout & Marked Pointers

```c
#define COBALT_LFSKIPLIST_MAX_LEVEL 32
#define COBALT_LFSKIPLIST_MARK_BIT  ((uintptr_t)0x1)

#define COBALT_IS_MARKED(ptr)       (((uintptr_t)(ptr) & COBALT_LFSKIPLIST_MARK_BIT) != 0)
#define COBALT_GET_UNMARKED(ptr)    ((cobalt_lfskiplist_node_t *)((uintptr_t)(ptr) & ~COBALT_LFSKIPLIST_MARK_BIT))
#define COBALT_GET_MARKED(ptr)      ((cobalt_lfskiplist_node_t *)((uintptr_t)(ptr) | COBALT_LFSKIPLIST_MARK_BIT))

typedef struct cobalt_lfskiplist_node {
    void                           *key;
    _Atomic(void *)                 value;
    uint8_t                         top_level; // 1..32
    _Atomic(uintptr_t)              next[];    // flexible array member of size top_level
} cobalt_lfskiplist_node_t;

typedef struct cobalt_lfskiplist {
    cobalt_lfskiplist_node_t       *head;
    compare_func_t                  compar;
    cobalt_allocator_t             *allocator;
    cobalt_ebr_t                   *ebr;
    bool                            owns_ebr;
    atomic_size_t                   size;
} cobalt_lfskiplist_t;
```

### 4.2 Algorithm & Concurrency Invariants

1. **Wait-Free Search (`cobalt_lfskiplist_get`)**:
   - Readers traverse from `top_level - 1` down to level 0.
   - At each node, readers load `next[level]` with `memory_order_acquire`.
   - Marked nodes are simply skipped using `COBALT_GET_UNMARKED(next)`.
   - If `compar(curr->key, key) == 0` and node is unmarked, atomic load `value` and return success.
   - Zero CAS operations, zero atomic writes, 100% wait-free.

2. **Lock-Free Insert (`cobalt_lfskiplist_insert`)**:
   - Height $L$ sampled using geometric distribution:
     ```c
     uint8_t level = 1;
     while (level < COBALT_LFSKIPLIST_MAX_LEVEL && (xorshift32(&seed) & 1)) {
         level++;
     }
     ```
   - Invokes `find_preds_succs(key, preds, succs)` to locate insertion position across all levels.
   - Allocates new node with height $L$.
   - **Linearization Point**: CAS `&preds[0]->next[0]` from `succs[0]` to `new_node`.
   - Once level 0 CAS succeeds, the node is logically inserted.
   - Progressively links levels $1 \dots L-1$ using CAS:
     - If upper-level CAS fails due to concurrent modification, calls `find_preds_succs` to refresh predecessors and retries.
     - If the new node itself was concurrently marked for deletion, terminates upper-level linking immediately.

3. **Two-Phase Lock-Free Remove (`cobalt_lfskiplist_remove`)**:
   - **Phase 1: Logical Deletion**:
     - Traverses from node's `top_level - 1` down to 0.
     - For each level $i$, attempts CAS `&victim->next[i]` from `succ` to `COBALT_GET_MARKED(succ)`.
     - **Linearization Point**: Level 0 CAS marking succeeds. Node is immediately logically removed.
   - **Phase 2: Physical Unlinking & EBR Retirement**:
     - Calls `find_preds_succs` to cooperatively swing `preds[i]->next[i]` past `victim`.
     - Submits `victim` to `cobalt_ebr_retire(handle, victim, node_free_fn, skiplist->allocator)`.
     - Decrements atomic `size`.

4. **Cooperative Physical Unlink in `find_preds_succs`**:
   - While traversing level $i$, if `curr->next[i]` is marked:
     - Computes `succ = COBALT_GET_UNMARKED(curr->next[i])`.
     - Attempts CAS `&pred->next[i]` from `curr` to `succ`.
     - If CAS fails, re-reads or restarts search to guarantee consistency.

---

## 5. Range Scan & Iterator Specification

### 5.1 Hybrid Guard Semantics
- Single point calls (`get`, `insert`, `remove`, `put`, `contains`) acquire a thread-local cached EBR handle automatically:
  ```c
  cobalt_ebr_handle_t *h = get_or_register_tls_handle(skiplist->ebr);
  cobalt_ebr_enter(h);
  // ... perform point operation ...
  cobalt_ebr_exit(h);
  ```
- Range scans traverse multiple nodes over extended execution time. An iterator pins the EBR epoch upon creation, ensuring all traversed nodes remain valid in memory even if concurrently removed by other threads:
  ```c
  cobalt_lfskiplist_iter_t *it = cobalt_lfskiplist_iter_create(skiplist, min_key, max_key);
  void *k, *v;
  while (cobalt_lfskiplist_iter_next(it, &k, &v)) {
      // Safely access k and v without locks
  }
  cobalt_lfskiplist_iter_destroy(it); // unpins EBR epoch
  ```

### 5.2 Iterator Data Structure

```c
struct cobalt_lfskiplist_iter {
    cobalt_lfskiplist_t      *skiplist;
    cobalt_ebr_handle_t      *handle;
    cobalt_lfskiplist_node_t *curr;
    const void               *max_key;
    bool                      finished;
};
```

---

## 6. Error Handling & Edge Cases

| Edge Case | Defense & Handling |
|---|---|
| `NULL` arguments (`!skiplist`, `!key`, `!compar`) | Return explicit error codes (`-1` or `-2`) with zero state mutation. |
| Node allocation failure (`allocator` returns `NULL`) | Abort cleanly, return `-2`, leaving the SkipList completely intact. |
| High contention CAS loops | Apply exponential backoff with `cobalt_cpu_pause()` after 8 failed attempts. |
| Concurrent insert and delete on identical key | Linearized strictly at Level 0 CAS. Winner proceeds; loser detects mark or existing key and acts appropriately. |
| Thread abruptly terminates while in EBR | POSIX pthread thread-specific destructor intercepts termination and auto-unregisters handle, transferring retired nodes. |
| Range iterator created with `min_key > max_key` | Instantly returns an empty iterator where `iter_next` yields `false`. |

---

## 7. Testing & Quality Gate Plan

### 7.1 Unit Tests
- `tests/unit/test_ebr.c`:
  - `test_ebr_lifecycle`: Create, register, enter/exit, destroy.
  - `test_ebr_delayed_reclaim`: Verify nodes retired in epoch $e$ are freed strictly at $e + 2$.
  - `test_ebr_multithread_advance`: 8 threads concurrently pinning/unpinning and validating monotonic epoch advance.
  - `test_ebr_thread_exit`: Verify handle cleanup on thread termination.
- `tests/unit/test_lfskiplist.c`:
  - `test_lfskiplist_basic`: Insert, get, contains, remove, put (upsert), size on 10,000 keys.
  - `test_lfskiplist_edge_cases`: Empty list, single element, duplicates, NULL args.
  - `test_lfskiplist_range_scan`: Bounded range, half-open range, full scan, empty ranges.
  - `test_lfskiplist_custom_allocator`: Verify arena and tracking allocator integration.
  - `test_lfskiplist_struct_keys`: 16-byte composite keys (`test_u128_t`) and string keys.

### 7.2 Multi-Threaded Concurrency Stress Test
- 8 worker threads executing 1,000,000 mixed operations:
  - 70% `get`
  - 20% `insert` / `put`
  - 10% `remove`
- Validated under AddressSanitizer (ASan) and LeakSanitizer (LSan) with 0 memory leaks, 0 races, 0 crashes.

### 7.3 Performance Benchmark Suite (`tests/benchmark/benchmark_lfskiplist.c`)
- Compare `cobalt_lfskiplist_t` against mutex-wrapped `cobalt_treemap_t`:
  - Benchmark 1: Read-Heavy (90% read / 10% write) across 1M operations (expecting 4x~8x speedup).
  - Benchmark 2: High-contention concurrent inserts across 8 threads.
  - Benchmark 3: Multi-threaded range scan throughput.
- Integrated into unified `bench_runner` (human-readable and `--json` modes).

---

## 8. Verification & Release Criteria

1. 100% CTest pass rate across all unit tests and benchmark runners.
2. 0 compiler warnings under `-Wall -Wextra -Werror` in C11 mode.
3. 0 memory leaks and 0 memory errors under AddressSanitizer and LeakSanitizer.
4. Clean static analysis under `clang-tidy` and clean formatting under `clang-format`.
5. Version bumped to `v2.8.0` with documentation updated in `CHANGELOG.md`, `README.md`, and `docs/SPEC/collections.md`.
