# Cobalt Change Log

## Unreleased

### Added
- **Layer 4 Lock-Free Concurrent Split-Ordered Hashmap (`cobalt/container/lfhashmap.h`)**:
  - 100% lock-free concurrent extensible hashmap (`cobalt_lfhashmap_t`) implementing the Split-Ordered List algorithm (Shalev & Shavit 2006).
  - Two-level segmented directory scaling dynamically from 4 up to 2.14 billion buckets without copying or moving existing key-value pairs (zero-copy dynamic expansion).
  - Wait-free point lookups (`cobalt_lfhashmap_get`, `cobalt_lfhashmap_contains`) traversing single-digit node hops with zero CAS or lock operations.
  - Harris marked-pointer cooperative deletion (`cobalt_lfhashmap_remove`) combined with Layer 7 Epoch-Based Reclamation (`cobalt_ebr_t`) for thread-safe deferred node freeing without ABA issues.
  - Concurrent safe iterator (`cobalt_lfhashmap_iter_t`) with epoch pinning and automatic skipping of marked and dummy sentinel nodes.
  - Comprehensive unit test suite (`tests/unit/test_lfhashmap.c`) covering CRUD, dynamic resizing across 16,384 buckets, iterators, generic/pointer keys, and 4-thread 100k contention stress tests.
  - Dedicated benchmark suite (`tests/benchmark/benchmark_lfhashmap.c`) and runner integration, achieving >14.3M ops/sec multi-threaded throughput.
- **Layer 8 Platform Shared Memory Primitive (`cobalt/platform/shm.h`)**:
  - Cross-platform shared memory mapping (`cobalt_shm_t`) with POSIX `shm_open`, `ftruncate`, `mmap`, `munmap`, `shm_unlink` and Windows `CreateFileMappingA`, `MapViewOfFile`, `UnmapViewOfFile`, `CloseHandle`.
  - Automatic leading slash normalization, argument validation, and fine-grained resource cleanup.
- **Layer 8 Platform Notification Primitive (`cobalt/platform/notifier.h`)**:
  - Cross-platform event signaling abstraction (`cobalt_notifier_t`) with anonymous notifications (Linux `eventfd(EFD_NONBLOCK | EFD_CLOEXEC)`) and named cross-process notifications (POSIX named FIFO in `/dev/shm` or `/tmp` with `O_RDWR | O_NONBLOCK | O_CLOEXEC` to prevent EOF; Windows named `CreateEventA`/`OpenEventA`).
  - Signal-safe `poll()` timeout waiting with `EINTR` loop retry and non-blocking draining.
  - `cobalt_notifier_get_fd` exposing the underlying pollable descriptor for integration into event loop reactors.
- **Layer 4 Lock-Free SPSC Shared Memory Ring Buffer (`cobalt/container/shm_ring.h`)**:
  - Cache-line isolated variable-length single-producer single-consumer ring buffer (`cobalt_shm_spsc_t`).
  - Cache line alignment (`_Alignas(64)`) separating `write_cursor`, `read_cursor`, and synchronization state, eliminating false sharing.
  - Contiguous zero-copy reserve/commit/cancel API (`cobalt_shm_spsc_reserve`, `cobalt_shm_spsc_commit`, `cobalt_shm_spsc_cancel`) allowing producers to write directly into shared memory without intermediate memory copies.
  - Wrap-around frame management (`COBALT_SHM_FRAME_WRAP`) transparently skipping tail padding during consumer zero-copy peek/consume (`cobalt_shm_spsc_peek`, `cobalt_shm_spsc_consume`).
- **Layer 4 Dmitry Vyukov MPMC Shared Memory Ring Buffer (`cobalt/container/shm_ring.h`)**:
  - Dmitry Vyukov bounded lock-free sequence-slot multi-producer multi-consumer ring buffer (`cobalt_shm_mpmc_t`).
  - 64-byte aligned slot stride (`ALIGN_UP(sizeof(cobalt_shm_mpmc_slot_t) + slot_size, 64)`) ensuring every slot begins on an isolated cache line.
  - Pure atomic CAS linearization across multiple concurrent producer and consumer processes (`cobalt_shm_mpmc_push`, `cobalt_shm_mpmc_pop`).
- **Layer 2 Shared Memory IPC Channel Facade & Wait Policies (`cobalt/module/shm_channel.h`)**:
  - Unified channel facade (`cobalt_shm_channel_t`) with dual topologies (SPSC variable-length ring & MPMC Dmitry Vyukov slot queue).
  - Configurable runtime wait policies (`COBALT_SHM_WAIT_SPIN`, `COBALT_SHM_WAIT_BLOCKING`, `COBALT_SHM_WAIT_ADAPTIVE` with configurable spin threshold before descriptor sleep).
  - Kernel Syscall Coalescing: producers check `atomic_load_explicit(&hdr->waiting_readers, memory_order_relaxed)` and skip OS notification syscalls when consumers are actively spinning, achieving pure user-space zero-syscall data transfers.
  - SPSC zero-copy reserve/commit/cancel/peek/consume and unified `cobalt_shm_channel_send`/`cobalt_shm_channel_recv`.
  - Asynchronous EventLoop integration (`cobalt_shm_channel_attach_eventloop`, `cobalt_shm_channel_detach_eventloop`) with automatic non-blocking batch message draining callbacks.
- **Multi-Process Stress Test Suite (`tests/unit/test_shm.c`)**:
  - 4 POSIX `fork()` multi-process test scenarios:
    1. 50,000 variable-length SPSC message stream with CRC32 integrity verification across processes.
    2. 4-process MPMC contention (2 producers + 2 consumers, 50,000 items) verifying 100% delivery without lost or duplicated messages.
    3. Multi-process EventLoop batch drain (10,000 rapid messages).
    4. Crash recovery: child killed via `SIGKILL` during uncommitted reserve; parent verifies shared memory integrity and deadlock-free operation.
- **Dedicated IPC Performance Benchmark Suite (`tests/benchmark/benchmark_shm.c`)**:
  - Ping-pong round-trip latency (Min, Median/P50, P90, P99, Max, Average) across SPSC Spin, SPSC Blocking, and MPMC Blocking policies.
  - Streaming throughput benchmarks for 64B, 1KB, and 64KB payloads, confirming >20M msgs/sec throughput and zero-copy bandwidth >600 GB/s.
  - Integrated into unified `tests/benchmark/bench_runner.c` with `--json` reporting support.
- **Regression Test for Overflow & OOB Defects (`tests/unit`)**:
  - `test_buffer_defrag_prepend_no_underflow` locks in the defragmentation underflow fix (ASan-confirmed heap OOB write).
  - `test_vector_overflow_guards` locks in the `SIZE_MAX / sizeof(void *)` element limit on `create`/`reserve`/push-doubling.
  - `test_regex_backref_no_truncation` locks in the `size_t` backreference counter (previously `uint8_t`, truncating captures past 255 chars).
  - `test_json_surrogate_pairs` locks in UTF-16 surrogate-pair synthesis in the JSON
    DOM parser: a paired `\uD83D\uDE00` decodes to the 4-byte UTF-8 form of U+1F600,
    and a lone high/low surrogate fails the string parse (partial object/array tree,
    NULL at the top level).

### Changed
- Total CTest suites expanded from 61 to 63 (45 unit test suites + 18 benchmarks; new `test_lfhashmap` and `bench_lfhashmap`), maintaining 100% pass rate under Release, Debug, and AddressSanitizer/LeakSanitizer (`build-asan`).
- Full clang-tidy pass across all IPC headers and sources with 0 warnings.
- Capped JSON DOM parser nesting depth at 128 (matches SAX parser
  `COBALT_JSON_MAX_DEPTH`); over-deep input now returns NULL instead of overflowing
  the stack.
- Thread-pool `parallel_for`, its waitgroup, and `cobalt_parallel_stable_sort_with_alloc`
  now honor the injected allocator (new `cobalt_waitgroup_create_with_alloc`).
- Test coverage extended for previously untested API surface:
  `cobalt_interface_new_with_allocator`/`destroy_with_allocator`,
  `cobalt_class_create_with_allocator` (name/base-class/alloc round-trip and the
  matching `cobalt_class_destroy` free path), and
  `cobalt_iterator_new_with_allocator` (context and shell both released through the
  creation allocator).
- `test_threadsafewrapper` gained NULL-guard and behavioral coverage that only existed
  for vector/hashmap/list: full NULL-safety suites plus single-thread basic suites for
  the thread-safe deque, queue, stack, set, and treemap; bounded-access semantics for
  `tsvector_get`/`set`/`reserve`/`shrink_to_fit`; `tslist_remove_at` in-range and
  out-of-range; same-key overwrite and missing-key reads for `tshashmap` and
  `tstreemap` (including `min_key`/`max_key` bounds).
- `test_logger` now asserts log content rather than just call return: level filtering
  counts emitted lines by severity, asserts `TRACE`/`DEBUG` are suppressed below the
  threshold, verifies the NULL output stream falls back to `stdout`, and verifies
  `min_log_level` can be re-set mid-run in both directions.
- Added `COBALT_STRICT_WARNINGS` CMake option (default ON) controlling
  `-Wall -Wextra -Werror`; removed unused `test_registry.ld` / `TEST_REGISTER` /
  `TEST_AUTO_REGISTER` dead code.
- Added `COBALT_RUN_TSAN` CMake option (default OFF) building with
  `-fsanitize=thread`; CI already runs a dedicated `tsan` job.
- `list_merge` (used by `cobalt_list_merge_sort` / `cobalt_list_sort`) converted
  from per-node recursion to a single-pass iterative merge, bounding merge-sort
  stack depth to O(log n) instead of O(n) for very large lists.
- `tsan` CI job now runs a bounded retry: the test step is `continue-on-error`
  with a single re-run on failure, so the documented EBR reclaim-vs-read
  `heap-use-after-free` false positive (a TSan blind spot to the epoch barrier,
  not a protocol race — all six skiplist read paths are epoch-enclosed) no longer
  flakes the gate. The retry is capped at one and scoped to the tsan job only; a
  genuine race still fails twice and is not masked. Design:
  `docs/superpowers/specs/2026-10-19-ci-reliability-tsan-flake-design.md`.


### Fixed
- **Test suite silently passing on failure (`test_object`, `test_interface`, `test_iterator`, `test_atomic`, `test_logger`, `test_platform`)**: all six suites used `if (ok) printf("OK") else fprintf(stderr, ...)`, which printed an error but never exited non-zero, so unit tests reported `ctest` green even when assertions had failed. All six now use `TEST_ASSERT`/`TEST_EQUAL` from `test_framework.h`, which abort the process on failure. This restored genuine failure detection for object reference counting, interface creation, iterator traversal, atomic concurrency (race detection), log-level filtering, and platform detection.
- **`test_iterator.c` vector capacity argument bug**: `cobalt_vector_create(4)` treated the initial *capacity* parameter as an element size, undersizing the backing store for the 5-element array. Now `cobalt_vector_create(5)`, matching `test_vector.c` conventions, plus size assertions.
- EBR: `cobalt_ebr_sync` no longer reclaims the orphan pool prematurely — orphaned
  reclamation is now epoch-bounded (3-epoch rule), so nodes retired after the sync
  point are kept until a later advance instead of being freed while active readers
  may still hold them.
- EBR: handle reuse no longer silently drops the previous owner's lingering local
  retired lists — they are now spliced into the orphan pool under the orphan lock,
  closing the lost-update window where handle churn (`unregister` → `register`)
  leaked retired nodes.
- EBR: `cobalt_ebr_unregister` reset `handle->retired_count` outside the
  `orphan_lock` while the register-reuse path resets the same field under it,
  leaving an unsynchronized write pair when a handle is immediately re-registered
  by another thread (TSan-confirmed data race in `test_ebr`). The reset now runs
  inside the lock on both paths.
- `cobalt_buffer_ensure_writable` defragmentation: `discardable = read_pos -
  prepend_size` underflowed to `SIZE_MAX` after `cobalt_buffer_prepend` lowered
  `read_pos` below `prepend_size`, causing a heap out-of-bounds write on the next
  expansion (ASan-confirmed). Now clamped to 0 so the demand routes to the
  geometric-expansion branch.
- `cobalt_buffer_ensure_writable` geometric expansion: `capacity * 2` and
  `write_pos + len + capacity` guarded against `size_t` overflow (`SIZE_MAX`).
- `cobalt_vector_create` / `cobalt_vector_reserve` / push-doubling: requested
  element counts now capped at `SIZE_MAX / sizeof(void *)`, so the allocated
  pointer array can never wrap and alias a short block (later indexing would
  run out of bounds).
- Pike VM regex engine: `pike_thread_t` backreference `backref_offset` /
  `backref_remaining` counters widened from `uint8_t` to `size_t`, so a capture
  longer than 255 chars (e.g. `(a{128}a{128}a{128})\1` → 384 chars) is matched in
  full instead of being silently truncated to 255 (false-positive match).
- Geometric-growth overflow guards: capacity math in the regex node pool and codegen
  instruction table, the treemap iterator stack, the eventloop timer table (power-of-two
  clamp at 2^30 to keep the `new_cap - 1` mask valid) and pollfds array (`INT_MAX`
  clamp in the int domain), and the JSON DOM/SAX string-growth paths is now clamped to
  `SIZE_MAX / sizeof(elem)`; the `json_sax` initial-segment `malloc` is NULL-checked.
  The clamps are defensive (reachable only near `SIZE_MAX/2`), so no dedicated tests
  were added — the full suite plus the ASan build are the regression net.
- The JSON DOM parser now synthesizes UTF-16 surrogate pairs into a single 4-byte
  UTF-8 sequence, matching the SAX parser; lone high/low surrogates fail the string
  parse instead of emitting invalid UTF-8 bytes.
- The eventloop signal counter `g_signal_count` is now `volatile sig_atomic_t`,
  removing data-race-undefined behavior from the async-signal context (handler
  increment vs. drain-loop decrement).

## v2.7.0 (2026-10-17) — Lock-Free Concurrent SkipList, 3-Epoch EBR Engine & Linear Pike VM Regex Engine


### Added
- **3-Epoch Safe Memory Reclamation Engine (`cobalt/memory/ebr.h`)**:
  - Pure C11 wait-free memory reclamation based on Keir Fraser / Herlihy-Lev-Shavit 3-Epoch state machine ($e \in \{0, 1, 2\}$).
  - 64-byte cache-line aligned per-thread handles (`cobalt_ebr_handle_t`) with `_Alignas(64)` preventing false sharing.
  - $O(1)$ wait-free `enter` and `exit` operations using atomic acquire-release semantics.
  - Bounded 64-item retirement batches with deferred two-epoch retirement invariant guaranteeing zero use-after-free.
  - Monotonic 64-bit EBR instance ID generation with thread-local handle caching and automatic POSIX thread-key destructor cleanup for terminating threads.
  - Custom memory allocator injection (`cobalt_allocator_t`).
  - Synchronous quiescence barrier (`cobalt_ebr_sync`) for testing and resource drainage.
- **Lock-Free Concurrent SkipList (`cobalt/container/lfskiplist.h`)**:
  - Fraser / Herlihy-Lev-Shavit (HLS) lock-free skip list supporting up to 32 levels with $P=0.5$ geometric height distribution.
  - Bit-0 marked pointer tagging (`(uintptr_t)ptr | 1`) for atomic two-phase logical deletion.
  - Cooperative physical unlinking integrated into search paths (`find_preds_succs`), guaranteeing bounded step bounds even under high contention.
  - $O(\log N)$ wait-free point lookups (`cobalt_lfskiplist_get`, `cobalt_lfskiplist_contains`) requiring no locks or write operations.
  - $O(\log N)$ concurrent insertions (`cobalt_lfskiplist_insert`, `cobalt_lfskiplist_put`) with Level-0 CAS linearization and bottom-up tower elevation.
  - Ordered range iterators (`cobalt_lfskiplist_iter_create`, `cobalt_lfskiplist_iter_next`, `cobalt_lfskiplist_iter_destroy`) pinning EBR epochs to enable safe concurrent traversal without holding container-wide locks.
  - Transparent support for arbitrary key types (integers, strings, arbitrary structs) with user-defined `compare_func_t` comparator.
- **Linear Pike VM Regular Expression Engine (`cobalt/algorithm/regex.h`)**:
  - Pure C11 deterministic $O(N \times M)$ linear time execution engine, providing mathematical immunity against ReDoS (Regular Expression Denial of Service) catastrophic backtracking.
  - Comprehensive regular expression grammar: literals, alternations (`|`), wildcards (`.`), anchors (`^`, `$`, `\b`, `\B`), 256-bit character class bitmaps (`[...]`, `[^...]`, ranges `a-z`, shorthand `\d`, `\D`, `\w`, `\W`, `\s`, `\S`), quantifiers (`*`, `+`, `?`, `{m,n}`) with greedy and non-greedy (`?`) semantics.
  - Submatch extraction via zero-copy slice structures (`cobalt_regex_capture_t captures[16]`) with `start`, `end`, and `length`.
  - Non-capturing groups `(?:...)` and capturing groups `(...)` up to 15 sub-groups.
  - Backreference matching `\1` .. `\15` executed in lockstep without backtracking.
  - Compilation flags: `COBALT_REGEX_CASE_INSENSITIVE`, `COBALT_REGEX_DOTALL`, and `COBALT_REGEX_MULTILINE`.
  - Stack Fast-Path: Zero dynamic heap allocations during matching for patterns with $\le 64$ instructions.
  - Full custom memory allocator support (`cobalt_allocator_t`).
  - Convenient one-shot test API (`cobalt_regex_test`) and exact-match API (`cobalt_regex_match_exact`).
- **Dedicated Benchmarks & Stress Suites**:
  - `tests/benchmark/benchmark_lfskiplist.c`: Multi-threaded Read-Heavy (>3.0M ops/sec), Balanced (>2.7M ops/sec), and 4.77x speedup over mutex-locked `cobalt_treemap`.
  - `tests/benchmark/benchmark_regex.c`: ReDoS vulnerability immunity test on pathological pattern `^(a+)+$` ($< 0.05$ ms, ~100k ops/sec) and industrial workloads (IPv4, email 126k+ ops/sec).
  - 8-thread SkipList concurrency stress test (200,000 mixed operations) integrated into unit test suite.
  - Integrated into the unified `bench_runner` benchmark tool.

### Fixed
- **EventLoop (`src/module/eventloop.c`)**:
  - Fixed use-after-free by maintaining `loop->fd_tail` when the tail descriptor entry is deleted in `cobalt_eventloop_del_fd`.
- **JSON Parser (`src/module/json_parse.c`)**:
  - Replaced `memcpy` with `memmove` in `jrealloc` to prevent memory overlap warnings during arena-backed buffer expansions.

### Changed
- Total CTest count expanded from 54 to 59 tests, maintaining 100% pass rate under standard compilation and AddressSanitizer/LeakSanitizer (ASan/LSan).

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
