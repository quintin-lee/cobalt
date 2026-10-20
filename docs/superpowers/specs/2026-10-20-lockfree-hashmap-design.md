# Cobalt Lock-Free Concurrent Hashmap Design Specification

- **Module**: Layer 4 Container (`cobalt/container/lfhashmap.h`, `src/container/lfhashmap.c`)
- **Author**: Antigravity Pair Programmer
- **Date**: 2026-10-20
- **Status**: Approved

---

## 1. Overview & Motivation

Cobalt provides foundational lock-free data structures including the Michael-Scott queue (`cobalt_lfqueue_t`) and the Fraser / Herlihy-Lev-Shavit lock-free skip list (`cobalt_lfskiplist_t`), both operating with Layer 7 Epoch-Based Reclamation (`cobalt_ebr_t`).

However, Cobalt lacks a concurrent $O(1)$ lock-free key-value container. The existing `cobalt_hashmap_t` uses Robin Hood open addressing designed strictly for single-threaded usage. Wrapping a traditional hash table with mutual exclusion locks introduces contention bottlenecks, thread priority inversions, and blocking during dynamic table expansion.

This specification introduces **`cobalt_lfhashmap_t`**, an industrial-grade, 100% lock-free concurrent extensible hash table based on the **Split-Ordered List** algorithm (Ori Shalev and Nir Shavit, 2006). It provides:
1. **Wait-Free Point Reads**: Lookups traverse nodes using atomic loads without taking locks, CAS operations, or memory modifications.
2. **Lock-Free Dynamic Resizing**: Resizing from $N$ to $2N$ buckets requires **zero data element copying or migration**; elements naturally reside in their correct positions within a single globally ordered linked list.
3. **Two-Level Segmented Directory**: A 32-entry top-level directory indexing geometrically sized segments scales from 4 to 2.14 billion buckets without reallocating existing arrays.
4. **Harris Marked-Pointer Deletion & EBR Memory Safety**: Uses bit-0 tagged pointers for atomic two-phase logical deletion and physical unlinking, backed by Layer 7 `cobalt_ebr_t` for safe deferred node reclamation.
5. **Dual Interface Strategy**: A generic API supporting custom hash/equality functions (with default string FNV-1a support) alongside an ultra-fast inline path (`cobalt_lfhashmap_create_ptr`) for integer and pointer keys.

---

## 2. Core Architecture & Mathematical Foundation

### 2.1 The Split-Ordering Principle

In a classical hash table with capacity $N = 2^k$, a key's bucket index is:
$$\text{bucket} = \text{hash} \ \& \ (N - 1)$$

When the capacity doubles to $2N = 2^{k+1}$, the element either stays in bucket $B$ or moves to bucket $B + 2^k$, determined solely by the $(k+1)$-th bit of the hash code.

In a Split-Ordered List, every key is assigned a **Split-Order Key (`so_key`)**, defined as the 64-bit bit-reversal of its hash code:
$$\text{so\_key} = \text{reverse\_bits64}(\text{hash})$$

Because the lowest bits of the hash code become the most significant bits (MSBs) of `so_key`:
1. Keys belonging to bucket $B$ share the same bit-reversed prefix of length $k$.
2. In a single sorted linked list ordered by `so_key`, all keys belonging to bucket $B$ are naturally clustered together.
3. When bucket $B$ splits into bucket $B$ and bucket $B + 2^k$, the split point in the linked list is exact, and **no existing data nodes need to be moved**.

### 2.2 Distinguishing Sentinel (Dummy) and Regular Nodes

To index buckets directly into the global list, each initialized bucket has a **Sentinel (Dummy) Node**.
Bit 63 (the most significant bit) serves as the node type discriminator:

- **Dummy Sentinel Node for Bucket $B$**:
  $$\text{so\_key} = \text{reverse\_bits64}(B) \ \& \ \sim(1\text{ULL} \ll 63)$$
  (Bit 63 is forced to 0; user key and value are NULL.)
- **Regular Data Node**:
  $$\text{so\_key} = \text{reverse\_bits64}(\text{hash}) \mid (1\text{ULL} \ll 63)$$
  (Bit 63 is forced to 1; stores user key and atomic value.)

This invariant guarantees:
$$\text{so\_key}(\text{Dummy}_B) < \text{so\_key}(\text{Regular}_{B, i}) < \text{so\_key}(\text{Dummy}_{B'})$$
Every regular node in bucket $B$ is sorted strictly after the dummy node of bucket $B$, and strictly before the dummy node of any higher-ordered bucket.

```
Global Linked List Order:
[Dummy 0] -> [Data (hash ends with 00)] -> [Dummy 2] -> [Data (hash ends with 10)] -> [Dummy 1] -> [Data (hash ends with 01)] -> [Dummy 3] ...
```

---

## 3. Data Structures & Memory Layout

### 3.1 Node Structure (`cobalt_lfhashmap_node_t`)

```c
#define COBALT_LFHASHMAP_MARK_BIT ((uintptr_t)0x1)

#define COBALT_IS_MARKED(ptr)     (((uintptr_t)(ptr) & COBALT_LFHASHMAP_MARK_BIT) != 0)
#define COBALT_GET_UNMARKED(ptr)  ((void *)((uintptr_t)(ptr) & ~COBALT_LFHASHMAP_MARK_BIT))
#define COBALT_GET_MARKED(ptr)    ((void *)((uintptr_t)(ptr) | COBALT_LFHASHMAP_MARK_BIT))

typedef struct cobalt_lfhashmap_node {
    uint64_t            so_key; /* Bit-reversed split-order key */
    void               *key;    /* User key pointer (NULL for dummy nodes) */
    _Atomic(void *)     value;  /* User value pointer (NULL for dummy nodes) */
    _Atomic(uintptr_t)  next;   /* Next node pointer tagged with bit-0 mark */
} cobalt_lfhashmap_node_t;
```

### 3.2 Two-Level Segmented Directory

Rather than allocating a single monolithic bucket pointer array that must be reallocated and copied upon expansion, `cobalt_lfhashmap_t` uses a two-level segmented directory:

```c
#define COBALT_LFHASHMAP_MAX_SEGMENTS 32
#define COBALT_LFHASHMAP_INIT_BUCKETS 4

struct cobalt_lfhashmap {
    /* Top-level directory of 32 segments */
    _Atomic(cobalt_lfhashmap_node_t **) segments[COBALT_LFHASHMAP_MAX_SEGMENTS];

    atomic_size_t       count;         /* Number of active regular elements */
    atomic_size_t       bucket_count;  /* Current logical bucket capacity (power of 2) */

    cobalt_lfhash_func_t  hash_func;   /* Hash function callback */
    cobalt_lfequal_func_t equal_func;  /* Equality comparison callback */
    bool                  fast_ptr_mode;/* True for inline pointer/integer key mode */

    cobalt_allocator_t *allocator;     /* Injectable memory allocator */
    cobalt_ebr_t       *ebr;           /* Epoch-Based Reclamation engine */
    bool                owns_ebr;      /* True if EBR engine was created internally */
};
```

### 3.3 Segment Sizing & Bucket Location Math

The directory segments follow geometric progression:
- Segment 0: Size 2 (covers buckets 0..1)
- Segment 1: Size 2 (covers buckets 2..3)
- Segment 2: Size 4 (covers buckets 4..7)
- Segment $k$ ($k \ge 1$): Size $2^k$, covering buckets $2^k \dots 2^{k+1} - 1$.

Total addressable capacity with 32 segments:
$$2 + \sum_{k=1}^{31} 2^k = 2^{32} \text{ buckets}$$

Given any bucket index `B`:
- If `B < 2`: `segment = 0`, `offset = B`.
- If `B >= 2`:
  - Let $k = 63 - \text{\_\_builtin\_clzll}(B)$ (the leading 1-bit index).
  - `segment = k`
  - `offset = B ^ (1ULL << k)` (masking out the leading 1-bit yields the segment offset).

This calculation executes in 3 CPU cycles with zero branching.

---

## 4. Algorithms & Concurrency Protocols

### 4.1 Bit Reversal

A fast 64-bit bit-reversal primitive:
```c
static inline uint64_t reverse_bits64(uint64_t v) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bitreverse64(v);
#else
    v = ((v >> 1) & 0x5555555555555555ULL) | ((v & 0x5555555555555555ULL) << 1);
    v = ((v >> 2) & 0x3333333333333333ULL) | ((v & 0x3333333333333333ULL) << 2);
    v = ((v >> 4) & 0x0F0F0F0F0F0F0F0FULL) | ((v & 0x0F0F0F0F0F0F0F0FULL) << 4);
    v = ((v >> 8) & 0x00FF00FF00FF00FFULL) | ((v & 0x00FF00FF00FF00FFULL) << 8);
    v = ((v >> 16) & 0x0000FFFF0000FFFFULL) | ((v & 0x0000FFFF0000FFFFULL) << 16);
    return (v >> 32) | (v << 32);
#endif
}
```

### 4.2 Lazy Bucket Initialization

A bucket pointer in the segmented directory may be uninitialized (NULL).
When bucket $B$ is accessed:
1. Load segment pointer `map->segments[seg]`. If NULL, allocate the segment array using `sl_alloc`, zero-initialize, and CAS into `map->segments[seg]`. If CAS fails, free the allocated segment and load the winner's pointer.
2. Load `bucket_node = segment[offset]`.
3. If `bucket_node != NULL`, return `bucket_node`.
4. If NULL:
   - Identify parent bucket: `parent_idx = B ^ (1ULL << (63 - __builtin_clzll(B)))`.
   - Recursively call `get_or_init_bucket(map, parent_idx)` to ensure the parent is initialized.
   - Allocate a new Dummy node with `so_key = reverse_bits64(B) & ~MSB`.
   - Traverse the linked list starting from the parent dummy node, using lock-free CAS to insert the new dummy node in split-order.
   - If CAS fails due to another thread inserting the same dummy node, discard the local dummy node and reuse the existing one.
   - Store the dummy node pointer into `segment[offset]` via atomic store.

### 4.3 List Traversal with Cooperative Deletion (`find_pred_curr`)

During mutations (insert, put, remove), threads cooperatively assist with physical unlinking of logically deleted nodes:
1. Start at bucket sentinel `head`.
2. Walk forwards tracking `pred` and `curr = COBALT_GET_UNMARKED(pred->next)`.
3. Check `curr->next`:
   - If `COBALT_IS_MARKED(curr->next)`:
     - The node is logically deleted.
     - Attempt to physically unlink: `atomic_compare_exchange_weak(&pred->next, &curr, COBALT_GET_UNMARKED(curr->next))`.
     - If unlinking succeeds, retire `curr` to EBR: `cobalt_ebr_retire(handle, curr, node_free_fn, map)`.
     - If CAS fails, retry traversal from `head`.
4. Continue until `curr == NULL` or `curr->so_key >= target_so_key`.

### 4.4 Operation Implementations

#### `cobalt_lfhashmap_get(map, key, out_value)` — Wait-Free
1. Call `cobalt_ebr_enter(handle)` to pin active epoch.
2. Compute `hash = compute_hash(map, key)` and `so_key = reverse_bits64(hash) | MSB`.
3. `bucket_idx = hash & (atomic_load_explicit(&map->bucket_count, memory_order_relaxed) - 1)`.
4. `head = get_or_init_bucket(map, bucket_idx)`.
5. Walk from `head` while `curr != NULL && curr->so_key <= so_key`:
   - If `curr->so_key == so_key` and keys match:
     - Load `val = atomic_load_explicit(&curr->value, memory_order_acquire)`.
     - Check `curr->next`: if not marked, set `*out_value = val`, call `cobalt_ebr_exit(handle)`, return 0 (`COBALT_SUCCESS`).
   - Advance `curr = COBALT_GET_UNMARKED(curr->next)`.
6. Call `cobalt_ebr_exit(handle)`, return `COBALT_ERROR_NOT_FOUND`.

#### `cobalt_lfhashmap_insert(map, key, value)`
1. Pin EBR epoch.
2. Compute `hash`, `so_key`, `bucket_idx`, and get bucket `head`.
3. Loop:
   - Call `find_pred_curr(head, so_key, key, &pred, &curr)`.
   - If matching key found and unmarked: unpin EBR, return `COBALT_ERROR_ALREADY_EXISTS`.
   - Allocate `new_node`, set `so_key`, `key`, `value`, `next = curr`.
   - CAS `&pred->next, &curr -> new_node`.
   - If CAS succeeds:
     - `new_count = atomic_fetch_add_explicit(&map->count, 1, memory_order_relaxed) + 1`.
     - If `new_count / atomic_load(&map->bucket_count) > 0.75`: trigger doubling via CAS on `bucket_count`.
     - Unpin EBR, return `COBALT_SUCCESS`.
   - If CAS fails, free `new_node` and retry.

#### `cobalt_lfhashmap_put(map, key, value, out_old_value)`
1. Pin EBR epoch.
2. Loop:
   - Call `find_pred_curr(head, so_key, key, &pred, &curr)`.
   - If matching key found and unmarked:
     - Update value via CAS: `atomic_compare_exchange_weak(&curr->value, &old_val, value)`.
     - If CAS succeeds: set `*out_old_value = old_val`, unpin EBR, return 0.
   - If key not found: perform insertion as in `insert`.

#### `cobalt_lfhashmap_remove(map, key, out_old_value)`
1. Pin EBR epoch.
2. Loop:
   - Call `find_pred_curr(head, so_key, key, &pred, &curr)`.
   - If key not found or marked: unpin EBR, return `COBALT_ERROR_NOT_FOUND`.
   - **Phase 1 (Logical Delete)**:
     - Tag `curr->next` with `COBALT_LFHASHMAP_MARK_BIT` via CAS:
       `atomic_compare_exchange_strong(&curr->next, &unmarked_next, COBALT_GET_MARKED(unmarked_next))`.
     - If CAS fails, retry search.
   - **Phase 2 (Physical Unlink & Retire)**:
     - This is the linearization point.
     - `atomic_fetch_sub_explicit(&map->count, 1, memory_order_relaxed)`.
     - Read old value into `out_old_value`.
     - Cooperatively unlink: `atomic_compare_exchange_weak(&pred->next, &curr, unmarked_next)`.
     - Retire node: `cobalt_ebr_retire(handle, curr, node_free_fn, map)`.
     - Unpin EBR, return `COBALT_SUCCESS`.

---

## 5. Public API Specification (`cobalt/container/lfhashmap.h`)

```c
#ifndef COBALT_CONTAINER_LFHASHMAP_H
#define COBALT_CONTAINER_LFHASHMAP_H

#include "cobalt/memory/allocator.h"
#include "cobalt/memory/ebr.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cobalt_lfhashmap cobalt_lfhashmap_t;
typedef struct cobalt_lfhashmap_iter cobalt_lfhashmap_iter_t;

typedef struct cobalt_lfhashmap_guard {
    cobalt_ebr_handle_t *handle;
} cobalt_lfhashmap_guard_t;

typedef uint64_t (*cobalt_lfhash_func_t)(const void *key);
typedef int      (*cobalt_lfequal_func_t)(const void *a, const void *b);

/* Lifecycle */
cobalt_lfhashmap_t *cobalt_lfhashmap_create(
    cobalt_lfhash_func_t  hash_fn,
    cobalt_lfequal_func_t equal_fn
);

cobalt_lfhashmap_t *cobalt_lfhashmap_create_ptr(void);

cobalt_lfhashmap_t *cobalt_lfhashmap_create_with_allocator(
    cobalt_lfhash_func_t  hash_fn,
    cobalt_lfequal_func_t equal_fn,
    cobalt_allocator_t   *alloc,
    cobalt_ebr_t         *ebr,
    bool                  fast_ptr_mode
);

void cobalt_lfhashmap_destroy(cobalt_lfhashmap_t *map);

/* Core CRUD */
int  cobalt_lfhashmap_insert(cobalt_lfhashmap_t *map, void *key, void *value);
int  cobalt_lfhashmap_put(cobalt_lfhashmap_t *map, void *key, void *value, void **out_old_value);
int  cobalt_lfhashmap_get(cobalt_lfhashmap_t *map, const void *key, void **out_value);
bool cobalt_lfhashmap_contains(cobalt_lfhashmap_t *map, const void *key);
int  cobalt_lfhashmap_remove(cobalt_lfhashmap_t *map, const void *key, void **out_old_value);

/* Metadata */
size_t cobalt_lfhashmap_size(const cobalt_lfhashmap_t *map);
size_t cobalt_lfhashmap_bucket_count(const cobalt_lfhashmap_t *map);
bool   cobalt_lfhashmap_is_empty(const cobalt_lfhashmap_t *map);

/* Batch Guard */
cobalt_lfhashmap_guard_t cobalt_lfhashmap_pin(cobalt_lfhashmap_t *map);
void                     cobalt_lfhashmap_unpin(cobalt_lfhashmap_guard_t *guard);

/* Iterator */
cobalt_lfhashmap_iter_t *cobalt_lfhashmap_iter_create(cobalt_lfhashmap_t *map);
bool                     cobalt_lfhashmap_iter_next(cobalt_lfhashmap_iter_t *iter, void **out_key, void **out_value);
void                     cobalt_lfhashmap_iter_destroy(cobalt_lfhashmap_iter_t *iter);

#ifdef __cplusplus
}
#endif

#endif /* COBALT_CONTAINER_LFHASHMAP_H */
```

---

## 6. Verification & Quality Gates

### 6.1 Unit Test Suite (`tests/unit/test_lfhashmap.c`)
- **Lifecycle & Argument Validation**: Validates create/destroy across custom and system allocators, invalid pointers, edge cases.
- **Basic CRUD**: Tests insert, get, contains, put (with old value return), remove across both string keys and integer/pointer keys.
- **Dynamic Resizing**: Inserts 10,000 items sequentially and randomly, forcing multiple bucket table doublings, asserting all elements remain reachable.
- **Iterator Correctness**: Verifies that iteration over the full map visits exactly `size` elements and skips all dummy sentinel nodes.
- **Concurrency Stress Test**: 4 threads executing 25,000 mixed read/write/remove operations each (100,000 total operations) with atomic checksum and count verification.

### 6.2 Performance Benchmark (`tests/benchmark/benchmark_lfhashmap.c`)
- **Read-Dominant Workload**: 90% Get, 10% Put; verifies wait-free point read throughput (>50M ops/sec on modern x86/ARM64).
- **Write-Heavy Workload**: 50% Insert, 50% Remove.
- **Multi-Threaded Scaling**: 1, 2, 4, 8 threads benchmarked against standard mutex-locked hashmap.
- **Resizing Latency Distribution**: Confirms zero latency spike during bucket doublings.
- Integrated into `tests/benchmark/bench_runner.c` with `--json` reporting support.

### 6.3 Sanitizer & CI Gates
- **AddressSanitizer & LeakSanitizer**: Zero memory leaks, zero buffer overflows.
- **UndefinedBehaviorSanitizer**: Zero misaligned accesses, zero signed integer overflows.
- **ThreadSanitizer & Valgrind**: Clean execution under data race detectors.
- **Cross-Platform**: Tested on Linux (GCC/Clang), macOS (AppleClang/GCC), Windows (MinGW-w64).
