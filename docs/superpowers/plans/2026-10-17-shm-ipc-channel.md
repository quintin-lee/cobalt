# Implementation Plan: Lock-Free Zero-Copy Shared Memory IPC Channel (`cobalt_shm_channel_t`)

- **Design Spec**: `docs/superpowers/specs/2026-10-17-shm-ipc-channel-design.md`
- **Target Release**: Cobalt v2.8.0
- **Base Commit**: `34c61d7`
- **Architecture**: Layer 8 (Platform) $\to$ Layer 4 (Container) $\to$ Layer 2 (Module)

---

## Task Breakdown

### Task 1: Layer 8 Platform Shared Memory Primitive (`cobalt_shm_t`)
- **Files**:
  - `include/cobalt/platform/shm.h` (declare `cobalt_shm_t` lifecycle and mapping)
  - `src/platform/shm.c` (implement POSIX `shm_open`/`mmap` and Windows `CreateFileMapping`)
  - `CMakeLists.txt` (wire `src/platform/shm.c`, add `-lrt` on Linux if needed)
- **Details**:
  - `cobalt_shm_create(const char *name, size_t size, cobalt_shm_t **out_shm)`
  - `cobalt_shm_open(const char *name, cobalt_shm_t **out_shm)`
  - `void *cobalt_shm_get_addr(cobalt_shm_t *shm)`
  - `size_t cobalt_shm_get_size(const cobalt_shm_t *shm)`
  - `void cobalt_shm_close(cobalt_shm_t *shm)`
  - `int cobalt_shm_unlink(const char *name)`
  - Strict input validation, safe page size alignment, memory cleanup.
- **Verification**: Basic sanity test in initial `tests/unit/test_shm.c`.

---

### Task 2: Layer 8 Platform Notification Primitive (`cobalt_notifier_t`)
- **Files**:
  - `include/cobalt/platform/notifier.h` (declare `cobalt_notifier_t` wait/notify API)
  - `src/platform/notifier.c` (implement Linux `eventfd`, macOS/POSIX `socketpair`/`pipe`, Windows `Event`)
  - `CMakeLists.txt` (wire `src/platform/notifier.c`)
- **Details**:
  - `cobalt_notifier_create(const char *name, cobalt_notifier_t **out_notif)`
  - `cobalt_notifier_open(const char *name, cobalt_notifier_t **out_notif)`
  - `int cobalt_notifier_notify(cobalt_notifier_t *notif)` (increment count / set event)
  - `int cobalt_notifier_wait(cobalt_notifier_t *notif, int timeout_ms)` (sleep until notified or timeout)
  - `int cobalt_notifier_get_fd(const cobalt_notifier_t *notif)` (for epoll/poll integration)
  - `void cobalt_notifier_destroy(cobalt_notifier_t *notif)`
- **Verification**: Verify notify, non-blocking poll, timed wait, and descriptor readiness.

---

### Task 3: Layer 4 Lock-Free SPSC Shared Memory Ring (`cobalt_shm_spsc_t`)
- **Files**:
  - `include/cobalt/container/shm_ring.h` (declare SPSC and MPMC ring structures and protocols)
  - `src/container/shm_ring.c` (implement SPSC variable-length zero-copy ring logic)
  - `CMakeLists.txt` (wire `src/container/shm_ring.c`)
- **Details**:
  - `cobalt_shm_header_t` initialization and attachment validation (magic, version, capacity power of 2).
  - Cacheline-isolated write and read cursors (`_Alignas(64)`).
  - Two-phase zero-copy `reserve` & `commit`:
    - Handles wrap-around contiguous boundary: writes `COBALT_SHM_FRAME_WRAP` marker and wraps to offset 0.
    - Zero CAS overhead on data path: acquire load on consumer cursor, release store on producer cursor.
  - Two-phase zero-copy `peek` & `consume`:
    - Transparently skips wrap marker frames.
    - Returns contiguous payload pointer and advances reader cursor.
- **Verification**: Unit tests covering reserve/commit, wrap-around boundary, ring full/empty conditions.

---

### Task 4: Layer 4 Dmitry Vyukov MPMC Shared Memory Ring (`cobalt_shm_mpmc_t`)
- **Files**:
  - `include/cobalt/container/shm_ring.h` (declare MPMC ring operations)
  - `src/container/shm_ring.c` (implement Dmitry Vyukov sequence-slot algorithm)
- **Details**:
  - Bounded slot array: each slot has `_Alignas(64) atomic_uint_fast64_t sequence` and payload.
  - Multi-producer `cobalt_shm_mpmc_push`:
    - Load `write_cursor`, inspect slot sequence, CAS `write_cursor`, write payload, store `sequence = pos + 1` with release barrier.
  - Multi-consumer `cobalt_shm_mpmc_pop`:
    - Load `read_cursor`, inspect slot sequence, CAS `read_cursor`, read payload, store `sequence = pos + capacity` with release barrier.
- **Verification**: Unit tests for MPMC single and multi-threaded enqueue/dequeue correctness.

---

### Task 5: Layer 2 Channel Facade, Wait Policies & EventLoop Integration (`cobalt_shm_channel_t`)
- **Files**:
  - `include/cobalt/module/shm_channel.h` (declare public channel API)
  - `src/module/shm_channel.c` (implement channel lifecycle, wait state machine, EventLoop bridge)
  - `include/cobalt/cobalt.h` (include `shm_channel.h`)
  - `CMakeLists.txt` (wire `src/module/shm_channel.c`)
- **Details**:
  - `cobalt_shm_channel_create` & `cobalt_shm_channel_open` & `cobalt_shm_channel_destroy`.
  - Wait policy implementation:
    - `COBALT_SHM_WAIT_SPIN`: `_mm_pause()` / yield in spin loop, 0 syscalls.
    - `COBALT_SHM_WAIT_BLOCKING`: sleep on `cobalt_notifier_wait`.
    - `COBALT_SHM_WAIT_ADAPTIVE`: spin for `spin_threshold` iterations then sleep on notifier.
  - Syscall coalescing: check `waiting_readers` atomic counter; skip OS notification if receiver is active.
  - `cobalt_shm_channel_attach_eventloop`: registers `read_fd` into `cobalt_eventloop_t` and performs batch drain on wake.
- **Verification**: Unit tests verifying wait policies, timeout behavior, and EventLoop integration.

---

### Task 6: Multi-Process Stress & Robustness Test Suite (`tests/unit/test_shm.c`)
- **Files**:
  - `tests/unit/test_shm.c`
  - `tests/test_runner.c` (wire `test_shm`)
  - `tests/CMakeLists.txt`
- **Details**:
  - Implement POSIX `fork()` multi-process test harness.
  - 7 comprehensive tests:
    1. `test_shm_spsc_basic`: Zero-copy reserve/commit & peek/consume.
    2. `test_shm_spsc_wraparound`: Ring boundary wraparound with padding.
    3. `test_shm_spsc_perf_loop`: 100,000 messages ping-pong with CRC32 checksum.
    4. `test_shm_mpmc_contention`: 4 producers + 4 consumers transferring 500,000 messages concurrently.
    5. `test_shm_eventloop_drain`: 20,000 messages dispatched to `cobalt_eventloop_t` with batch draining.
    6. `test_shm_wait_policies`: Verifying CPU usage and signaling across SPIN / BLOCKING / ADAPTIVE.
    7. `test_shm_orphan_cleanup`: Simulating ungraceful process crash (`SIGKILL`) and verifying safe recovery/unlinking.
- **Verification**: `ctest -R test_shm --output-on-failure`.

---

### Task 7: IPC Performance Benchmark Suite (`tests/benchmark/benchmark_shm.c`)
- **Files**:
  - `tests/benchmark/benchmark_shm.c`
  - `tests/benchmark/bench_runner.c` (wire `bench_shm_latency`, `bench_shm_throughput`)
  - `tests/CMakeLists.txt`
- **Details**:
  - Measure Round-Trip Latency (Min / Median / P99) with nanosecond precision (`clock_gettime(CLOCK_MONOTONIC)`).
  - Measure Throughput (M ops/s and GB/s) for 64B, 1KB, 64KB payloads.
  - Output standard benchmark summary and integrate into `--json` runner.
- **Verification**: Run `benchmark_shm` and `bench_runner` to confirm throughput $\ge$ 20M msgs/sec and latency $\le$ 80ns.

---

### Task 8: Code Quality, Memory Safety (ASan/LSan), Formatting & Release Documentation
- **Files**:
  - `CHANGELOG.md` (document Cobalt v2.8.0 Shared Memory IPC feature)
  - `README.md` (add IPC module section and performance summary)
- **Details**:
  - Run `clang-format` across all new headers, sources, and tests.
  - Run `clang-tidy` (`-DCOBALT_RUN_TIDY=ON`) ensuring 0 warnings.
  - Run full CTest suite ensuring 100% pass rate.
  - Run ASan/LSan build (`build-asan`) ensuring 0 memory leaks, 0 out-of-bounds accesses.
- **Verification**: Complete clean pass on all gates.
