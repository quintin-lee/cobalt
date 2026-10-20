# Lock-Free Concurrent Hashmap (Split-Ordered List) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a production-grade, 100% lock-free concurrent hashmap (`cobalt_lfhashmap_t`) in Layer 4 (Container) based on the Split-Ordered List algorithm with two-level segmented directory, zero-copy dynamic resizing, Wait-Free point reads, Harris marked-pointer deletions, Layer 7 `cobalt_ebr_t` safe memory reclamation, and full iterator support.

**Architecture:** Layer 4 `cobalt_lfhashmap_t` organizes key-value pairs into a single globally ordered lock-free singly linked list sorted by 64-bit bit-reversed hash values (`so_key`). Sentinel/Dummy nodes (MSB=0) index buckets, while regular nodes (MSB=1) store data. A 32-entry two-level segmented array directory scales logically from 4 up to 2.14 billion buckets without reallocating or copying existing elements. Deletions utilize atomic bit-0 pointer marking and cooperative unlinking, backed by `cobalt_ebr_t`.

**Tech Stack:** C11 (`stdatomic.h`, `_Alignas`, `stdbool.h`), CMake 3.14+, CTest, Clang-Format, Clang-Tidy, AddressSanitizer/LeakSanitizer, UndefinedBehaviorSanitizer, Valgrind.

---

## File Structure Map

| File Path | Role | Responsibility |
|---|---|---|
| `include/cobalt/container/lfhashmap.h` | Public Header (L4) | Declarations of `cobalt_lfhashmap_t`, `cobalt_lfhashmap_iter_t`, `cobalt_lfhashmap_guard_t`, lifecycle, CRUD, iterators, and fast-path APIs. |
| `src/container/lfhashmap.c` | Implementation (L4) | Bit-reversal math, two-level segmented directory, lazy bucket initialization, `find_pred_curr`, wait-free reads, insert, put, remove, iterator traversal. |
| `include/cobalt/cobalt.h` | Umbrella Header | Add `#include "cobalt/container/lfhashmap.h"`. |
| `tests/unit/test_lfhashmap.c` | Unit Test | Test lifecycle, CRUD, generic keys, fast-path pointer keys, 10k dynamic resizing, iterator, and 4-thread 100k concurrency stress test. |
| `tests/benchmark/benchmark_lfhashmap.c` | Benchmark | Read-heavy (90/10), write-heavy (50/50), multi-thread scaling (1/2/4/8 threads), and resizing latency. |
| `tests/benchmark/bench_runner.c` | Benchmark Runner | Integrate `benchmark_lfhashmap` into unified runner (human-readable and `--json` mode). |
| `tests/CMakeLists.txt` | Build Config | Register `benchmark_lfhashmap` executable and `bench_lfhashmap` CTest target. |
| `CMakeLists.txt` | Root Build Config | Add `src/container/lfhashmap.c` to `COBALT_SOURCES`. |

---

## Tasks

### Task 1: Public Header Declaration & Build Plumbing

**Files:**
- Create: `include/cobalt/container/lfhashmap.h`
- Create: `src/container/lfhashmap.c` (stub)
- Modify: `include/cobalt/cobalt.h`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create `include/cobalt/container/lfhashmap.h`**

```c
#ifndef COBALT_CONTAINER_LFHASHMAP_H
#define COBALT_CONTAINER_LFHASHMAP_H

/**
 * @file lfhashmap.h
 * @brief Lock-Free Concurrent Extensible Hashmap with Epoch-Based Reclamation
 * @details Implements a 100% lock-free concurrent hashmap using the Split-Ordered
 *          List algorithm (Shalev & Shavit, 2006). Features wait-free point reads,
 *          zero-copy dynamic bucket expansion via a two-level segmented directory,
 *          Harris marked-pointer two-phase deletion, and integration with Layer 7
 *          cobalt_ebr_t for safe deferred memory reclamation.
 *
 * @defgroup LFHASHMAP Lock-Free Hashmap
 * @ingroup Containers
 * @{
 */

#include "cobalt/memory/allocator.h"
#include "cobalt/memory/ebr.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COBALT_LFHASHMAP_MARK_BIT ((uintptr_t)0x1)

#define COBALT_IS_MARKED(ptr)     (((uintptr_t)(ptr) & COBALT_LFHASHMAP_MARK_BIT) != 0)
#define COBALT_GET_UNMARKED(ptr)  ((void *)((uintptr_t)(ptr) & ~COBALT_LFHASHMAP_MARK_BIT))
#define COBALT_GET_MARKED(ptr)    ((void *)((uintptr_t)(ptr) | COBALT_LFHASHMAP_MARK_BIT))

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

/** @} */

#endif /* COBALT_CONTAINER_LFHASHMAP_H */
```

- [ ] **Step 2: Create stub `src/container/lfhashmap.c`**
- [ ] **Step 3: Update `include/cobalt/cobalt.h` and `CMakeLists.txt`**
- [ ] **Step 4: Verify build succeeds** (`cmake --build build -j4`)

---

### Task 2: Core Algorithm, Two-Level Segmented Directory & CRUD

**Files:**
- Modify: `src/container/lfhashmap.c`

- [ ] **Step 1: Implement Bit-Reversal, Hash, and Key Equality Helpers**
  - Implement `reverse_bits64(v)` using `__builtin_bitreverse64` or portable 64-bit parallel shifts.
  - Implement default fast FNV-1a 64-bit string hash and `strcmp` equality.
  - Implement SplitMix64 inline hash for pointer/integer mode.
- [ ] **Step 2: Implement Two-Level Segmented Directory Math and Allocation**
  - `get_bucket_location(bucket_idx, &seg, &offset)` with `__builtin_clzll`.
  - Lazy segment allocation and atomic installation via CAS into `map->segments[seg]`.
- [ ] **Step 3: Implement Lazy Sentinel Bucket Initialization**
  - Parent calculation: `parent = B ^ (1ULL << (63 - __builtin_clzll(B)))`.
  - Recursive initialization of parent bucket.
  - Sentinel creation with `so_key = reverse_bits64(B) & ~MSB`.
  - Lock-free CAS insertion of sentinel into the global sorted linked list starting from parent.
- [ ] **Step 4: Implement `find_pred_curr` with Cooperative Physical Unlinking**
  - Traversal while `curr->so_key < target_so_key` or keys don't match.
  - Detecting marked `curr->next`, CAS unlinking, and `cobalt_ebr_retire`.
- [ ] **Step 5: Implement `insert`, `put`, `get`, `contains`, and `remove`**
  - Wait-Free `get` and `contains`.
  - `insert` with duplicate detection and dynamic load factor checking (`count / bucket_count > 0.75`).
  - `put` upsert with atomic CAS on `curr->value`.
  - `remove` with bit-0 marking on `curr->next`, decrementing `count`, cooperative unlinking, and `cobalt_ebr_retire`.
- [ ] **Step 6: Implement `cobalt_lfhashmap_destroy`**
  - Walk the global linked list from Dummy 0, free all nodes.
  - Free all allocated segments and destroy internal EBR if `owns_ebr`.

---

### Task 3: Iterator & Batch Guard Support

**Files:**
- Modify: `src/container/lfhashmap.c`

- [ ] **Step 1: Implement `cobalt_lfhashmap_pin` and `cobalt_lfhashmap_unpin`**
  - Acquire thread EBR handle, call `cobalt_ebr_enter(h)` and `cobalt_ebr_exit(h)`.
- [ ] **Step 2: Implement `cobalt_lfhashmap_iter_create`, `iter_next`, `iter_destroy`**
  - `iter_create`: Allocate iterator, enter EBR epoch, initialize `curr = map->segments[0][0]` (Dummy 0).
  - `iter_next`: Walk forwards across `curr = COBALT_GET_UNMARKED(curr->next)`.
  - Automatically skip all Dummy nodes (`(curr->so_key & (1ULL << 63)) == 0`).
  - Automatically skip all marked nodes (`COBALT_IS_MARKED(curr->next)`).
  - Return `*out_key = curr->key`, `*out_value = curr->value`.
  - `iter_destroy`: Exit EBR epoch, free iterator handle.

---

### Task 4: Comprehensive Unit Test Suite

**Files:**
- Create: `tests/unit/test_lfhashmap.c`
- Modify: `tests/test_runner.c`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create `tests/unit/test_lfhashmap.c`**
  - `test_lfhashmap_create_destroy`: Generic and pointer modes, allocator injection, NULL handling.
  - `test_lfhashmap_insert_get_contains`: Standard key-value insertion, lookup, duplicate rejection.
  - `test_lfhashmap_put`: Value updates, verifying old value returns.
  - `test_lfhashmap_remove`: Deletion of existing and non-existing keys, size decrement verification.
  - `test_lfhashmap_dynamic_resizing`: Sequential and random insertion of 10,000 keys causing multiple bucket doublings (from 4 up to 16,384 buckets), asserting 100% data accessibility.
  - `test_lfhashmap_iterator`: Verifying iterator visits all active elements, count equals `size`, sentinels skipped.
  - `test_lfhashmap_pin_unpin`: Verifying batch operations under guard.
  - `test_lfhashmap_concurrency_stress`: 4 worker threads performing 25,000 mixed operations each (100,000 total ops) with atomic checksum and count verification.
- [ ] **Step 2: Register test in `tests/test_runner.c` and `tests/CMakeLists.txt`**
- [ ] **Step 3: Run unit test** (`./build/tests/cobalt_test --filter lfhashmap`) and verify 100% pass.

---

### Task 5: Performance Benchmark Suite

**Files:**
- Create: `tests/benchmark/benchmark_lfhashmap.c`
- Modify: `tests/benchmark/bench_runner.c`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create `tests/benchmark/benchmark_lfhashmap.c`**
  - Benchmark 1: Read-dominant (90% Get, 10% Put) measuring M ops/sec.
  - Benchmark 2: Write-heavy (50% Insert, 50% Remove).
  - Benchmark 3: Concurrency scaling (1, 2, 4, 8 threads) measuring speedup ratio.
  - Benchmark 4: Resizing latency (measuring P50/P99 latency during bucket expansion).
- [ ] **Step 2: Integrate into `tests/benchmark/bench_runner.c`**
  - Add `bench_lfhashmap` to table and `--json` outputs.
- [ ] **Step 3: Register executable in `tests/CMakeLists.txt`**
- [ ] **Step 4: Execute benchmark** (`./build/tests/benchmark_lfhashmap`) and verify target performance (>50M ops/sec read throughput).

---

### Task 6: Quality Gates, Sanitizer Checks, and Documentation

**Files:**
- Modify: `CHANGELOG.md`
- Run: Clang-Format, ASan/LSan, UBSan, Valgrind

- [ ] **Step 1: Run `clang-format -i` across all new and modified files**
- [ ] **Step 2: Verify zero warnings under `-Wall -Wextra -Werror` (`COBALT_STRICT_WARNINGS=ON`)**
- [ ] **Step 3: Run AddressSanitizer/LeakSanitizer test** (`ctest --test-dir build-asan -R test_lfhashmap`)
- [ ] **Step 4: Run full CTest suite** (all 62 test cases pass)
- [ ] **Step 5: Update `CHANGELOG.md` under `## Unreleased`**
- [ ] **Step 6: Commit all changes with conventional gitmoji message**
