# Cobalt Change Log

## Unreleased

## v2.6.0 (2026-10-17) — Multi-Core Parallel Sorting & Event-Driven DAG TaskGraph Scheduler

### Added
- **Multi-Core Parallel Sorting Engine (`cobalt/module/parallel_sort.h`)**:
  - `cobalt_parallel_qsort` & `cobalt_parallel_qsort_with_cutoff`: Pure C11 in-place parallel PDQSort with adaptive cutoff (`max(4096, N / (4 * threads))`), Hoare multi-word partitioning, larger-partition threadpool task offloading, smaller-partition tail recursion, and Help-First WaitGroup stealing to completely eliminate recursive starvation deadlocks. Achieves up to 3.4x speedup on 8 threads.
  - `cobalt_parallel_stable_sort` & `cobalt_parallel_stable_sort_with_alloc`: 100% stable parallel TimSort-lite combining concurrent chunk sorting via `cobalt_threadpool_parallel_for` with pairwise binary tree merge barriers and left-run equality preference. Achieves up to 2.8x speedup on 4 threads. Supports custom allocator injection.
- **Event-Driven DAG TaskGraph Scheduler (`cobalt/module/taskgraph.h`)**:
  - Reusable Directed Acyclic Graph (DAG) task scheduler with zero reallocation across repeated runs.
  - Kahn's topological sort algorithm for compilation and acyclicity validation with cycle detection.
  - Pure event-driven push dispatch using atomic in-degree counter decrements (`atomic_fetch_sub_explicit(..., 1, memory_order_acq_rel)`), immediately pushing ready successors into the thread pool without central polling.
  - Help-First WaitGroup stealing barrier in `cobalt_taskgraph_run_and_wait` to guarantee starvation-free execution.
  - High-throughput execution: over 2,000,000 DAG tasks/sec in fan-out/fan-in and over 4,600,000 DAG tasks/sec in layered pipeline topologies.
- **Dedicated Benchmark Suite**:
  - Added `benchmark_parallel_sort` and `benchmark_taskgraph` executables and integrated them into unified `bench_runner` (supporting human-readable and `--json` outputs).
- Strict warning policy: CMake now enables `-Wall -Wextra -Werror` by default
- Documentation updates: README, API reference, and module specifications updated for Parallel Sort and TaskGraph

### Fixed
- Windows CI: fixed `test_thread` mutex trylock behavior and eventloop compilation/test guards
- macOS/Linux CI: fixed eventloop UNIX socket test callback placement and POSIX test guards
- Memory checks: fixed ASan/Valgrind issues in `test_threadsafewrapper` and allocator-inject tests
- Compiler warnings: removed unused functions/variables and fixed format/parameter warnings across tests and `eventloop.c`
- clang-tidy: removed redundant `cobalt_treemap_create_with_allocator` declaration in `treemap.c`

### Changed
- Total test count expanded from 50 to 54 tests, passing with 100% success under both standard CTest and AddressSanitizer/LeakSanitizer (ASan/LSan).
- Improved cross-platform test coverage with conditional compilation guards for Windows and POSIX-specific tests


## v2.5.0 (2026-10-17) — High-Performance Sorting, Work-Stealing Scheduler & Async Buffer/EventLoop Engine

### Added
- **Pure C11 Pattern-Defeating QuickSort (`cobalt_qsort`)**:
  - Implemented Tukey's ninther and median-of-3 pivot selection.
  - Dutch National Flag (DNF) 3-way partitioning (`< pivot`, `== pivot`, `> pivot`) delivering $O(N)$ performance on duplicate-heavy inputs.
  - Introspective Heapsort fallback on excessive recursion depth ($2 \lfloor \log_2 N \rfloor$).
  - In-place multi-word byte swap (`cobalt_swap_bytes`) executing word-aligned exchanges with zero heap allocations.
- **In-Place Introspective Heapsort (`cobalt_heapsort`)**:
  - Standalone max-heap sift-down sorting with zero dynamic allocations and guaranteed $O(N \log N)$ worst-case time complexity.
- **Adaptive Run MergeSort (`cobalt_stable_sort`, `cobalt_stable_sort_with_alloc`)**:
  - Natural run detection, in-place descending run reversal, and bidirectional scratch buffer merging with 100% stability.
  - Custom allocator injection via `cobalt_allocator_t`.
- **Zero-Alloc Partitioning (`cobalt_partition`, `cobalt_partition_with_alloc`)**:
  - Optimized Hoare partitioning with zero heap allocations on the default path while maintaining custom allocator tracking.
- **Lock-Free Work-Stealing Thread Pool (`cobalt_threadpool_t`)**:
  - Hybrid task scheduling: per-worker Chase-Lev SPMC lock-free deques (LIFO local execution, FIFO remote work-stealing) plus global bounded MPMC queue (`cobalt_lfqueue_t`) for external submissions.
  - Zero dynamic allocations on worker hot paths.
  - Synchronization barriers (`cobalt_waitgroup_t`) with Help-First work stealing to prevent starvation deadlocks in recursive fork-join tasks.
  - Dynamic chunked parallel loops (`cobalt_threadpool_parallel_for`).
  - Adaptive 3-stage idle backoff (spin-yield → idle registration → condvar wait).
- **Layer 4 Dynamic Zero-Copy Stream Buffer (`cobalt_buffer_t`)**:
  - 3-zone contiguous ring/slice buffer (prependable, readable, writable) with in-place defragmentation (`memmove`) and geometric expansion.
  - Zero-copy slicing via `cobalt_buffer_peek()` and `cobalt_buffer_retrieve()`.
  - Two-segment vectorized `readv` with 64KB stack buffer spillover in `cobalt_buffer_read_fd()`.
- **4-Level Hierarchical Timing Wheel**:
  - Replaced $O(\log N)$ min-heap with 4-level bitwise timing wheel (256 + 64 + 64 + 64 slots) with 1ms tick base resolution covering ~18.64 hours.
  - Intrusive doubly-linked timer nodes and Thomas Wang 64-bit mix hash table delivering strict $O(1)$ add and $O(1)$ cancellation.
  - Dynamic `epoll_wait` timeout computation scanning Level 0.
- **Cross-Thread Event Loop Task Dispatch (`cobalt_eventloop_post`)**:
  - Asynchronous task posting via lock-free queue with `eventfd` (Linux) / self-pipe (macOS/POSIX) wakeup.
  - Atomic syscall coalescing (`atomic_bool is_sleeping`) eliminating redundant write syscalls when the loop is already running.
- **High-Level Asynchronous Stream Abstraction (`cobalt_stream_t`)**:
  - Automatic `POLLOUT` event toggling on partial writes.
  - Direct-write fast path when output buffer is empty.
  - Callback hooks for read, write complete, and error/EOF handling.
- **Dedicated Benchmark Suite**:
  - Added `benchmark_sort`, `benchmark_threadpool`, and `benchmark_eventloop` executables, wired into unified `bench_runner` (supporting human-readable and `--json` outputs).

### Changed
- Total test count expanded from 41 to 50 tests, passing with 100% success under both standard CTest and AddressSanitizer/LeakSanitizer (ASan/LSan).
- Header `cobalt/cobalt.h` now includes `cobalt/container/buffer.h`.

## v2.4.0 (2026-09-05) — Allocator Support, OOM Fixes & Test Cleanup Release

### Added
- **JSON injectable allocator**: `cobalt_json_parse_with_alloc()`, `cobalt_json_serialize_with_alloc()`, and `cobalt_json_destroy_with_alloc()` for arena/custom-allocator support; `NULL` falls back to the system allocator and destroy must pair with the same allocator used at creation

### Fixed
- **OOM invisible in `cobalt_vector_push()`**: push now propagates allocation failure as `-1` per the header contract instead of always returning `0`
- **HashMap `remove_ext()` bucket index**: unified to the `& (bucket_count - 1)` bitmask used by put/get/remove
- **Sequence `add()` failure detection**: `cobalt_sequence_add()` now detects append failure via size diff without breaking the `void`-slot ABI

### Changed
- Wired two orphaned HashMap ext tests (`ext_int_keys`, `ext_null_callbacks`) into the test runner as regression coverage
- Untracked test and coverage artifacts (`Testing/`, `coverage.info`, `coverage_report/`) and added ignore rules

## v2.3.0 (2026-08-08) — Security Fixes & Advanced APIs Release

### Added
- **HashMap runtime hash/equal replacement**: `cobalt_hashmap_set_funcs()` for swapping hash and equality callbacks on an existing instance
- **TreeMap custom comparator**: `cobalt_treemap_create_ext()` with `cobalt_compare_func_t` for generic key comparison (not just strings)
- **Eventloop UNIX domain sockets**: `cobalt_eventloop_create_unix_server()` and `cobalt_eventloop_accept()` for local IPC
- **String utilities**: `cobalt_split()`, `cobalt_join()`, `cobalt_strip()` in `utils/string.h`
- **ASan suppressions**: Added `lsan.suppress` for known allocator-inject test infrastructure leaks

### Fixed
- **Heap-buffer-overflow in `cobalt_split()`**: Fixed allocation size miscalculation (operator precedence bug in `malloc`)
- **Memory leak in `test_thread_safety`**: Added proper cleanup of hashmap-allocated values
- **Double-free in `test_thread_safety`**: Removed premature `free(got)` before loop cleanup
- **CI shell line continuations**: Fixed `\\` → `\` in `.github/workflows/ci.yml` causing `command not found` errors
- **CI benchmark job**: Added `mkdir -p .benchmarks` before running benchmarks

### Changed
- HashMap now supports `cobalt_hashmap_create_ext()` for allocator-compatible creation
- TreeMap uses `rb_compare()` that delegates to configurable comparator
- Eventloop epoll dispatch fixed: `data.ptr` now correctly set instead of `data.fd`
- Updated test count: 25 → 41 (added thread safety, allocator inject, string tests)

### Security
- Fixed CWE-122 (heap buffer overflow) in `cobalt_split()` — allocation was `sizeof(char*)*cnt + 1` instead of `sizeof(char*)*(cnt+1)`

This project follows [Semantic Versioning](https://semver.org/) (MAJOR.MINOR.PATCH).

---

## v2.2.0 (2026-08-07) — Interface & Documentation Release

### Added
- **Sequence convenience API**: `cobalt_sequence_size`, `cobalt_sequence_is_empty`, `cobalt_sequence_add`, `cobalt_sequence_remove`
- **Deque implements `cobalt_sequence_t`**: polymorphic sequence usage for Deque
- **`docs/API/`**: Added core.md, memory.md, interfaces.md, runtime.md, platform.md
- **CI hardening**: Added format gate, ASan job, and Valgrind memory-check job

### Changed
- Moved `find_program` and `ALL_SRC_FILES` to top of CMakeLists.txt (fixes POST_BUILD format hang)
- Updated test count: 22 → 25

### Fixed
- fix(build): POST_BUILD format no longer hangs when variables are undefined
- style(container): fix readability-else-after-return in deque.c

---
## v2.1.0 (2026-08-02) — Feature Expansion Release

### Added
- **Set container**: Hash-based set with insert, remove, contains operations
- **Deque container**: Double-ended queue with push_front/back, pop_front/back
- **New algorithms**: binary search (cobalt_bsearch), find_if, for_each
- **JSON module split**: Separated json.c into json_parse.c and json_serialize.c
- **Shared string utility**: Consolidated my_strdup into cobalt_strdup()

### Changed
- Updated architecture documentation to reflect new containers and algorithms
- Updated code statistics (53 source/header files, 6627 lines, 83 commits)
- Updated test count: 22 tests passing

### Fixed
- Fixed duplicate compare_func_t typedef (now only in sort.h)
- Fixed header include order in cobalt.h

## v2.0.0 (2026-07-29) — Architecture Baseline Release

### Changed
- Complete 8-layer architecture implementation (L8 Platform → L1 Applications)
- Added core object system with single inheritance + multi-interface support
- Implemented container hierarchy: Sequence → Vector/List → Map → HashMap/Treemap
- Added algorithm layer with sorting and functional utilities
- Added JSON module and event loop module
- Created comprehensive documentation suite (RFC, SPEC, API, UML, DEV_GUIDE, EXAMPLES, TUTORIAL)

### Added
- `.gitignore` with AI tooling caches exclusion
- CMake build system with pkg-config and find_package support
- `.clangd` configuration for IDE integration
- All header files with include guards
- All source implementations (.c files)
- Specification documents for all 8 layers
- Developer guidelines and contribution workflow

### Fixed
- No bugs in initial baseline release

## v1.0.0 (TBD) — Initial Proposal

### Added
- Initial RFC document proposing the 8-layer architecture concept
- Foundation for object system design discussions

*Note: This is a development branch; public releases will be tagged separately.*
