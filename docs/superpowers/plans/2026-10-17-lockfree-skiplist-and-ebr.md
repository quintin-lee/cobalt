# Lock-Free SkipList with Epoch-Based Reclamation (EBR) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a production-grade, pure C11, zero-dependency concurrent data structure suite: a 3-Epoch Safe Memory Reclamation (EBR) engine in Layer 7 Memory and a pure Lock-Free Concurrent SkipList with marked pointers and ordered range scans in Layer 4 Container.

**Architecture:** Layer 7 `cobalt_ebr_t` provides 3-epoch cyclic rotation, 64-byte aligned per-thread handles, and local retirement batching (64 items) to safely eliminate UAF and ABA hazards. Layer 4 `cobalt_lfskiplist_t` implements the Fraser / Herlihy-Lev-Shavit (HLS) algorithm with 32 levels, bit-0 marked pointers for two-phase deletion, cooperative physical unlinking, wait-free point reads, and ordered range iterators with pinned EBR epochs.

**Tech Stack:** C11 (`stdatomic.h`, `_Alignas`, `stdbool.h`), POSIX threads (`pthread_key_t`), CMake 3.14+, CTest, Clang-Format, Clang-Tidy, AddressSanitizer/LeakSanitizer (ASan/LSan).

---

## File Structure Map

| File Path | Role | Responsibility |
|---|---|---|
| `include/cobalt/memory/ebr.h` | Public Header (L7) | Declarations of `cobalt_ebr_t`, `cobalt_ebr_handle_t`, lifecycle, enter/exit, and retire APIs. |
| `src/memory/ebr.c` | Implementation (L7) | 3-Epoch state machine, thread registry, batch advance, and POSIX thread-exit cleanup. |
| `include/cobalt/container/lfskiplist.h` | Public Header (L4) | Declarations of `cobalt_lfskiplist_t`, `cobalt_lfskiplist_iter_t`, point ops, range iterator. |
| `src/container/lfskiplist.c` | Implementation (L4) | SkipList node allocation, marked pointer CAS, find_preds_succs, wait-free get, insert, remove, iter. |
| `include/cobalt/cobalt.h` | Umbrella Header | Include `cobalt/memory/ebr.h` and `cobalt/container/lfskiplist.h`. |
| `tests/unit/test_ebr.c` | Unit Test | Test EBR lifecycle, 2-epoch retirement lag, multi-thread concurrent advance, and thread-exit cleanup. |
| `tests/unit/test_lfskiplist.c` | Unit Test | Test SkipList point ops, duplicate keys, edge cases, range scan, custom allocator, and 8-thread 1M stress test. |
| `tests/benchmark/benchmark_lfskiplist.c` | Benchmark | Read-heavy (90/10), balanced (50/50), concurrent inserts, range scan, and mutex treemap comparison. |
| `tests/benchmark/bench_runner.c` | Benchmark Runner | Integrate SkipList benchmarks into unified runner (text & JSON mode). |
| `tests/test_runner.c` | Test Runner | Register `test_ebr` and `test_lfskiplist` test suites. |
| `tests/CMakeLists.txt` | Build Config | Register unit tests and `benchmark_lfskiplist` CTest target. |
| `CMakeLists.txt` | Root Build Config | Add `src/memory/ebr.c` and `src/container/lfskiplist.c` to `COBALT_SOURCES`. |

---

## Tasks

### Task 1: Layer 7 `cobalt_ebr_t` Public API & Build Plumbing

**Files:**
- Create: `include/cobalt/memory/ebr.h`
- Create: `src/memory/ebr.c` (stub)
- Modify: `include/cobalt/cobalt.h`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create `include/cobalt/memory/ebr.h`**

```c
#ifndef COBALT_MEMORY_EBR_H
#define COBALT_MEMORY_EBR_H

/**
 * @file ebr.h
 * @brief Epoch-Based Reclamation (EBR) engine for lock-free data structures
 * @details Implements a 3-Epoch Safe Memory Reclamation (SMR) state machine.
 *          Provides wait-free registration, cacheline-aligned per-thread handles,
 *          and deferred batch reclamation to prevent Use-After-Free (UAF) and ABA hazards.
 *
 * @defgroup EBR Epoch-Based Reclamation
 * @ingroup Memory
 * @{
 */

#include "cobalt/memory/allocator.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cobalt_ebr cobalt_ebr_t;
typedef struct cobalt_ebr_handle cobalt_ebr_handle_t;

cobalt_ebr_t *cobalt_ebr_create(cobalt_allocator_t *alloc);
void cobalt_ebr_destroy(cobalt_ebr_t *ebr);

cobalt_ebr_handle_t *cobalt_ebr_register(cobalt_ebr_t *ebr);
void cobalt_ebr_unregister(cobalt_ebr_handle_t *handle);

void cobalt_ebr_enter(cobalt_ebr_handle_t *handle);
void cobalt_ebr_exit(cobalt_ebr_handle_t *handle);

void cobalt_ebr_retire(cobalt_ebr_handle_t *handle, void *ptr,
                       void (*free_fn)(void *ptr, void *user_data), void *user_data);

bool cobalt_ebr_try_advance(cobalt_ebr_t *ebr);
void cobalt_ebr_sync(cobalt_ebr_t *ebr);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* COBALT_MEMORY_EBR_H */
```

- [ ] **Step 2: Create stub `src/memory/ebr.c`**

Create stub implementations returning NULL / 0 for declarations in `include/cobalt/memory/ebr.h`.

- [ ] **Step 3: Modify `include/cobalt/cobalt.h` and `CMakeLists.txt`**

Add `#include "cobalt/memory/ebr.h"` to `include/cobalt/cobalt.h`.
Add `src/memory/ebr.c` to `COBALT_SOURCES` in `CMakeLists.txt`.

- [ ] **Step 4: Build and verify compilation**

Run: `cmake --build build -j4`
Expected: 0 compiler errors.

- [ ] **Step 5: Commit Task 1**

```bash
git add include/cobalt/memory/ebr.h src/memory/ebr.c include/cobalt/cobalt.h CMakeLists.txt
git commit -m "feat(ebr): ✨ declare public EBR memory reclamation API and build plumbing"
```

---

### Task 2: Layer 7 `cobalt_ebr_t` Engine Implementation & Unit Tests

**Files:**
- Modify: `src/memory/ebr.c`
- Create: `tests/unit/test_ebr.c`
- Modify: `tests/test_runner.c`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Implement full EBR in `src/memory/ebr.c`**

- Define `cobalt_ebr_node_t`, `cobalt_ebr_handle_t` with `_Alignas(64)`, and `cobalt_ebr_t`.
- Implement `cobalt_ebr_create` & `cobalt_ebr_destroy`.
- Implement lock-free handle registry using atomic CAS on `head_handle`.
- Implement `cobalt_ebr_enter` (`atomic_store_explicit(&handle->active_epoch, (g << 1) | 1, memory_order_seq_cst)`).
- Implement `cobalt_ebr_exit` (`atomic_store_explicit(&handle->active_epoch, 0, memory_order_release)`).
- Implement `cobalt_ebr_retire` pushing to `local_retired[g % 3]`.
- Implement batch threshold (64 items) auto-invoking `cobalt_ebr_try_advance`.
- Implement `cobalt_ebr_try_advance` checking all registered active handles; if all $\ge g$, advance `global_epoch` and reclaim epoch $(g + 2) \pmod 3$.
- Implement `cobalt_ebr_sync` advancing 2 full cycles to flush all retirements.
- Implement pthread thread-exit key destructor for automatic unregistration of orphaned handles.

- [ ] **Step 2: Create `tests/unit/test_ebr.c`**

Write unit tests:
1. `test_ebr_create_destroy`: Basic lifecycle and allocator check.
2. `test_ebr_enter_exit`: Single-thread enter and exit validation.
3. `test_ebr_delayed_reclaim`: Retire 100 items with custom counter free function; verify that items are NOT freed immediately, and strictly freed after advancing 2 epochs.
4. `test_ebr_multithread_advance`: 8 threads concurrently entering/exiting, ensuring monotonic epoch advancement and 0 race conditions.
5. `test_ebr_sync`: Verify `cobalt_ebr_sync` forces complete cleanup.

- [ ] **Step 3: Wire into `tests/test_runner.c` and `tests/CMakeLists.txt`**

Register `test_ebr` in `test_runner.c`.
Add `unit/test_ebr.c` to `tests/CMakeLists.txt`.

- [ ] **Step 4: Verify test suite**

Run: `cmake --build build -j4 && ./build/tests/cobalt_test --filter ebr`
Expected: All EBR unit tests pass.

- [ ] **Step 5: Commit Task 2**

```bash
git add src/memory/ebr.c tests/unit/test_ebr.c tests/test_runner.c tests/CMakeLists.txt
git commit -m "feat(ebr): ✨ implement 3-epoch safe memory reclamation engine"
```

---

### Task 3: Layer 4 `cobalt_lfskiplist_t` Public API & Build Plumbing

**Files:**
- Create: `include/cobalt/container/lfskiplist.h`
- Create: `src/container/lfskiplist.c` (stub)
- Modify: `include/cobalt/cobalt.h`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create `include/cobalt/container/lfskiplist.h`**

Declare full public API as designed in the spec:
- `cobalt_lfskiplist_create`
- `cobalt_lfskiplist_create_with_allocator`
- `cobalt_lfskiplist_destroy`
- `cobalt_lfskiplist_insert`
- `cobalt_lfskiplist_put` (upsert)
- `cobalt_lfskiplist_get`
- `cobalt_lfskiplist_contains`
- `cobalt_lfskiplist_remove`
- `cobalt_lfskiplist_size`
- `cobalt_lfskiplist_is_empty`
- `cobalt_lfskiplist_iter_create`
- `cobalt_lfskiplist_iter_next`
- `cobalt_lfskiplist_iter_destroy`
- `cobalt_lfskiplist_pin` & `cobalt_lfskiplist_unpin`

- [ ] **Step 2: Create stub `src/container/lfskiplist.c`**

Create stub implementations.

- [ ] **Step 3: Modify `include/cobalt/cobalt.h` and `CMakeLists.txt`**

Add `#include "cobalt/container/lfskiplist.h"` to `cobalt.h`.
Add `src/container/lfskiplist.c` to `CMakeLists.txt`.

- [ ] **Step 4: Build and verify compilation**

Run: `cmake --build build -j4`
Expected: 0 errors.

- [ ] **Step 5: Commit Task 3**

```bash
git add include/cobalt/container/lfskiplist.h src/container/lfskiplist.c include/cobalt/cobalt.h CMakeLists.txt
git commit -m "feat(skiplist): ✨ declare public lock-free skiplist API and build plumbing"
```

---

### Task 4: Pure Lock-Free SkipList Operations (Wait-Free Search, Insert & Marked Deletion)

**Files:**
- Modify: `src/container/lfskiplist.c`
- Create: `tests/unit/test_lfskiplist.c`
- Modify: `tests/test_runner.c`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Implement core SkipList algorithms in `src/container/lfskiplist.c`**

- Node structure with flexible array `_Atomic(uintptr_t) next[]`.
- Head sentinel node initialization with height 32.
- XorShift32 geometric distribution level generator.
- `find_preds_succs(skiplist, key, preds, succs)` with cooperative physical unlinking of marked nodes.
- `cobalt_lfskiplist_get` (Wait-Free fast path, skipping marked nodes without modifying list).
- `cobalt_lfskiplist_contains`.
- `cobalt_lfskiplist_insert`: CAS at Level 0 (linearization point), followed by upper-level CAS with mark check.
- `cobalt_lfskiplist_put`: Upsert semantics.
- `cobalt_lfskiplist_remove`: Two-phase deletion (Phase 1: mark Level 31..0; Phase 2: physical unlink via find and retire to EBR).
- Atomic `size` counter maintenance.
- TLS handle cache for automatic zero-parameter point operations.

- [ ] **Step 2: Create `tests/unit/test_lfskiplist.c`**

Write unit tests:
1. `test_lfskiplist_basic`: Insert 1,000 keys, verify `get`, `contains`, `size`.
2. `test_lfskiplist_duplicates`: Verify duplicate key rejection in `insert` vs overwrite in `put`.
3. `test_lfskiplist_remove`: Remove 500 keys, verify `get` returns -1, size decrements, and unremoved 500 keys remain intact.
4. `test_lfskiplist_edge_cases`: NULL arguments, empty list, single element list, missing keys.
5. `test_lfskiplist_struct_keys`: 16-byte structs (`test_u128_t`) with custom comparator.
6. `test_lfskiplist_custom_allocator`: Verify arena and custom allocator injection.

- [ ] **Step 3: Wire into test runner and build configs**

Register in `tests/test_runner.c` and `tests/CMakeLists.txt`.

- [ ] **Step 4: Verify test suite**

Run: `cmake --build build -j4 && ./build/tests/cobalt_test --filter lfskiplist`
Expected: All SkipList point tests pass.

- [ ] **Step 5: Commit Task 4**

```bash
git add src/container/lfskiplist.c tests/unit/test_lfskiplist.c tests/test_runner.c tests/CMakeLists.txt
git commit -m "feat(skiplist): ✨ implement lock-free search, insert, and marked-pointer deletion"
```

---

### Task 5: Ordered Range Scan Iterator & Pinned EBR Lifecycle

**Files:**
- Modify: `src/container/lfskiplist.c`
- Modify: `tests/unit/test_lfskiplist.c`

- [ ] **Step 1: Implement Range Iterator in `src/container/lfskiplist.c`**

- `cobalt_lfskiplist_iter_create`: Finds first node $\ge min\_key$ (or head->next[0] if NULL), pins EBR epoch via `cobalt_ebr_enter`.
- `cobalt_lfskiplist_iter_next`: Traverses `next[0]`, skips marked nodes, compares with `max_key`, yields key/value.
- `cobalt_lfskiplist_iter_destroy`: Unpins EBR epoch via `cobalt_ebr_exit` and frees iterator structure.
- `cobalt_lfskiplist_pin` & `cobalt_lfskiplist_unpin`.

- [ ] **Step 2: Add range scan tests in `tests/unit/test_lfskiplist.c`**

1. Full range scan `[NULL, NULL]`: Verify all keys visited in strictly ascending order.
2. Bounded range scan `[200, 800]`: Verify boundaries and count.
3. Half-open ranges `[min, NULL]` and `[NULL, max]`.
4. Empty range `[800, 200]` (`min > max`).
5. Concurrent insert while iterating: verify iterator continues safely without crash or corrupted pointers.

- [ ] **Step 3: Verify tests**

Run: `cmake --build build -j4 && ./build/tests/cobalt_test --filter lfskiplist`
Expected: All tests pass.

- [ ] **Step 4: Commit Task 5**

```bash
git add src/container/lfskiplist.c tests/unit/test_lfskiplist.c
git commit -m "feat(skiplist): ✨ implement safe ordered range iterator with pinned EBR"
```

---

### Task 6: High-Concurrency Stress Test & Dedicated Benchmark Suite

**Files:**
- Modify: `tests/unit/test_lfskiplist.c`
- Create: `tests/benchmark/benchmark_lfskiplist.c`
- Modify: `tests/benchmark/bench_runner.c`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add multi-threaded stress test in `tests/unit/test_lfskiplist.c`**

- 8 worker threads executing 1,000,000 mixed operations (70% read, 20% insert, 10% remove).
- Verify 0 data races, 0 crashes, and consistent final state.

- [ ] **Step 2: Create `tests/benchmark/benchmark_lfskiplist.c`**

- Benchmark 1: Read-Heavy throughput (90% get, 10% put) on 1,000,000 operations across 1, 2, 4, 8 threads.
- Benchmark 2: Balanced throughput (50% get, 50% put).
- Benchmark 3: Multi-threaded range scan throughput.
- Benchmark 4: Comparative baseline vs mutex-wrapped `cobalt_treemap` showing lock-free scaling speedup ($> 3\times$).
- Output summary and `printf("Benchmark completed\n");`.

- [ ] **Step 3: Wire into `tests/CMakeLists.txt` and `bench_runner.c`**

- Add `benchmark_lfskiplist` executable and `bench_lfskiplist` CTest.
- Wire into `tests/benchmark/bench_runner.c` in both human-readable and `--json` modes.

- [ ] **Step 4: Run benchmark and tests**

Run:
```bash
cmake --build build -j4
./build/tests/benchmark_lfskiplist
ctest --test-dir build --output-on-failure -R bench_lfskiplist
```
Expected: All benchmarks complete with $> 3\times$ speedup over mutex baseline.

- [ ] **Step 5: Commit Task 6**

```bash
git add tests/unit/test_lfskiplist.c tests/benchmark/benchmark_lfskiplist.c tests/benchmark/bench_runner.c tests/CMakeLists.txt
git commit -m "test(bench): ⚡ add benchmark suite and concurrency stress test for lock-free skiplist"
```

---

### Task 7: Quality Gate, Sanitizers (ASan/LSan), Documentation & Release

**Files:**
- Modify: `CMakeLists.txt` (bump version to v2.8.0)
- Modify: `CHANGELOG.md`
- Modify: `README.md`
- Modify: `docs/SPEC/collections.md`
- Modify: `docs/SPEC/memory.md`

- [ ] **Step 1: Format codebase with `clang-format`**

```bash
clang-format -i src/memory/ebr.c include/cobalt/memory/ebr.h src/container/lfskiplist.c include/cobalt/container/lfskiplist.h tests/unit/test_ebr.c tests/unit/test_lfskiplist.c tests/benchmark/benchmark_lfskiplist.c tests/benchmark/bench_runner.c
clang-format --dry-run -Werror src/memory/ebr.c include/cobalt/memory/ebr.h src/container/lfskiplist.c include/cobalt/container/lfskiplist.h tests/unit/test_ebr.c tests/unit/test_lfskiplist.c tests/benchmark/benchmark_lfskiplist.c tests/benchmark/bench_runner.c
```

- [ ] **Step 2: Run `clang-tidy`**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCOBALT_RUN_TIDY=ON
cmake --build build -j4
```
Expected: 0 warnings.

- [ ] **Step 3: Full CTest execution**

```bash
ctest --test-dir build --output-on-failure
```
Expected: 100% tests pass (all unit tests + benchmarks).

- [ ] **Step 4: AddressSanitizer & LeakSanitizer execution**

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCOBALT_ENABLE_ASAN=ON
cmake --build build-asan -j4
ctest --test-dir build-asan --output-on-failure
```
Expected: 100% tests pass with 0 memory leaks and 0 memory errors.

- [ ] **Step 5: Documentation & Version Bump**

- Bump version to `2.8.0` in `CMakeLists.txt`.
- Update `CHANGELOG.md`, `README.md`, `docs/SPEC/collections.md`, `docs/SPEC/memory.md`.

- [ ] **Step 6: Commit Task 7**

```bash
git add CMakeLists.txt CHANGELOG.md README.md docs/
git commit -m "chore(release): 🔖 finalize Lock-Free SkipList with EBR engine for Cobalt v2.8.0"
```
