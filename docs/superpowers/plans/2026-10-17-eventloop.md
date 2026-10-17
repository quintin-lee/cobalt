# Phase 3: EventLoop Timing Wheel & Zero-Copy Async Buffer Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Modernize Cobalt's networking and event-driven runtime by introducing a Layer 4 reusable dynamic zero-copy buffer engine (`cobalt_buffer_t`) and a Layer 2 high-performance reactor (`cobalt_eventloop_t`) equipped with an $O(1)$ 4-level hierarchical timing wheel, cross-thread `eventfd` async wakeup task queue, and non-blocking asynchronous stream abstraction (`cobalt_stream_t`).

**Architecture:**
- **Layer 4 (`cobalt_buffer_t`)**: 3-zone contiguous ring/slice buffer (prependable, readable, writable) with in-place defragmentation, zero-copy slicing, and two-segment vectorized `readv` with 64KB stack buffer.
- **Layer 2 Timing Wheel**: 4-level bitwise wheel (256 + 64 + 64 + 64 slots) with 1ms tick base resolution covering 18.64 hours in ~3.5 KB memory, with intrusive doubly-linked nodes enabling $O(1)$ add and cancel.
- **Cross-Thread Wakeup (`cobalt_eventloop_post`)**: Lock-free MPMC task queue (`cobalt_lfqueue_t`) backed by non-blocking `eventfd` / self-pipe with syscall coalescing (`atomic_bool is_sleeping`).
- **Async Stream (`cobalt_stream_t`)**: Buffered stream handling partial writes, automatic `POLLOUT` toggle, and direct-write fast paths.

**Tech Stack:** C11 (`stdatomic.h`, `_Thread_local`), Linux `epoll` + `eventfd` / macOS `kqueue` / POSIX `poll` + self-pipe, `readv`/`writev`, CMake, CTest, Clang-Tidy, Clang-Format, AddressSanitizer (ASan).

---

## File Structure

### Files to Create
- `include/cobalt/container/buffer.h`: Public API header for Layer 4 zero-copy dynamic buffer.
- `src/container/buffer.c`: Core implementation of `cobalt_buffer_t` and vectorized I/O.
- `tests/unit/test_buffer.c`: Comprehensive unit test suite for dynamic buffer operations.
- `tests/benchmark/benchmark_eventloop.c`: Standalone benchmark measuring timing wheel, buffer, and post throughput.

### Files to Modify
- `include/cobalt/module/eventloop.h`: Add `cobalt_eventloop_post`, `cobalt_stream_t`, and stream lifecycle/callbacks.
- `src/module/eventloop.c`: Replace min-heap with 4-level timing wheel, implement eventfd wakeup, and stream abstraction.
- `CMakeLists.txt`: Add `src/container/buffer.c` to `SOURCES`.
- `include/cobalt/cobalt.h`: Include `"cobalt/container/buffer.h"`.
- `tests/CMakeLists.txt`: Register `test_buffer`, `benchmark_eventloop`, and `bench_eventloop`.
- `tests/test_runner.c`: Wire `test_buffer` into test runner.
- `tests/unit/test_eventloop.c`: Add timing wheel stress, cross-thread post, and stream echo tests.
- `tests/benchmark/bench_runner.c`: Wire eventloop and buffer benchmarks into unified runner.

---

## Task 1: Layer 4 Dynamic Zero-Copy Buffer (`cobalt_buffer_t`)

**Files:**
- Create: `include/cobalt/container/buffer.h`
- Create: `src/container/buffer.c`
- Create: `tests/unit/test_buffer.c`
- Modify: `CMakeLists.txt`
- Modify: `include/cobalt/cobalt.h`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_runner.c`

- [x] **Step 1: Create `include/cobalt/container/buffer.h`**

Define `cobalt_buffer_t` interface with Doxygen documentation:
```c
#ifndef COBALT_CONTAINER_BUFFER_H
#define COBALT_CONTAINER_BUFFER_H

#include "cobalt/memory/allocator.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define COBALT_BUFFER_DEFAULT_INITIAL_CAPACITY 1024
#define COBALT_BUFFER_DEFAULT_PREPEND_SIZE 8

typedef struct cobalt_buffer cobalt_buffer_t;

cobalt_buffer_t *cobalt_buffer_create(size_t initial_capacity);
cobalt_buffer_t *cobalt_buffer_create_with_alloc(size_t initial_capacity,
                                                size_t prepend_size,
                                                cobalt_allocator_t *alloc);
void             cobalt_buffer_destroy(cobalt_buffer_t *buf);

size_t           cobalt_buffer_readable_bytes(const cobalt_buffer_t *buf);
size_t           cobalt_buffer_writable_bytes(const cobalt_buffer_t *buf);
size_t           cobalt_buffer_prependable_bytes(const cobalt_buffer_t *buf);
const char      *cobalt_buffer_peek(const cobalt_buffer_t *buf);

int              cobalt_buffer_retrieve(cobalt_buffer_t *buf, size_t len);
void             cobalt_buffer_retrieve_all(cobalt_buffer_t *buf);
int              cobalt_buffer_read(cobalt_buffer_t *buf, void *dst, size_t len);

int              cobalt_buffer_ensure_writable(cobalt_buffer_t *buf, size_t len);
int              cobalt_buffer_append(cobalt_buffer_t *buf, const void *data, size_t len);
int              cobalt_buffer_prepend(cobalt_buffer_t *buf, const void *data, size_t len);

ssize_t          cobalt_buffer_read_fd(cobalt_buffer_t *buf, int fd, int *err_out);
ssize_t          cobalt_buffer_write_fd(cobalt_buffer_t *buf, int fd, int *err_out);

#endif /* COBALT_CONTAINER_BUFFER_H */
```

- [x] **Step 2: Implement `src/container/buffer.c`**

Implement `cobalt_buffer_t` struct, layout management, in-place defragmentation (`memmove`), geometric expansion, and two-segment `readv` using stack buffer.

- [x] **Step 3: Create `tests/unit/test_buffer.c`**

Test creation, destruction, append, prepend, retrieve, retrieve_all, expansion, defragmentation, custom allocator lifecycle, and `read_fd`/`write_fd` over pipes.

- [x] **Step 4: Wire into build systems**

Add `src/container/buffer.c` to `CMakeLists.txt`, include in `include/cobalt/cobalt.h`, register `test_buffer` in `tests/CMakeLists.txt` and `tests/test_runner.c`.

- [x] **Step 5: Build, verify tests & commit**

```bash
clang-format -i include/cobalt/container/buffer.h src/container/buffer.c tests/unit/test_buffer.c
cmake --build build -j4 && ./build/tests/cobalt_test --filter buffer
git add include/cobalt/container/buffer.h src/container/buffer.c tests/unit/test_buffer.c CMakeLists.txt include/cobalt/cobalt.h tests/CMakeLists.txt tests/test_runner.c
git commit -m "feat(buffer): ✨ implement Layer 4 dynamic zero-copy buffer engine"
```

---

## Task 2: Layer 2 4-Level Hierarchical Timing Wheel Core Engine

**Files:**
- Modify: `src/module/eventloop.c`
- Modify: `tests/unit/test_eventloop.c`

- [x] **Step 1: Replace binary min-heap with 4-level timing wheel in `src/module/eventloop.c`**

Implement:
- Level 0: 256 slots, 1 ms per slot (0..255 ms).
- Level 1: 64 slots, 256 ms per slot (~16.38 s).
- Level 2: 64 slots, 16.384 s per slot (~17.47 min).
- Level 3: 64 slots, 1048.576 s per slot (~18.64 hours).
- Intrusive doubly-linked `cobalt_timer_node_t`.
- $O(1)$ `cobalt_eventloop_add_timer`: calculate delta, insert into wheel head.
- $O(1)$ `cobalt_eventloop_del_timer`: unlink node and release.
- Bitwise cascade on tick overflow.
- Dynamic `epoll_wait` timeout computation scanning Level 0.

- [x] **Step 2: Add comprehensive timing wheel tests in `tests/unit/test_eventloop.c`**

Add tests:
- 10,000 concurrent timers with varying delays across all 4 levels.
- Mid-flight random cancellation of 5,000 timers.
- Repeating timer accuracy check.
- Verify all existing eventloop tests continue to pass.

- [x] **Step 3: Build, verify tests & commit**

```bash
clang-format -i src/module/eventloop.c tests/unit/test_eventloop.c
cmake --build build -j4 && ./build/tests/cobalt_test --filter eventloop
git add src/module/eventloop.c tests/unit/test_eventloop.c
git commit -m "feat(eventloop): ✨ implement 4-level hierarchical timing wheel"
```

---

## Task 3: Cross-Thread Async Wakeup & Task Dispatch (`cobalt_eventloop_post`)

**Files:**
- Modify: `include/cobalt/module/eventloop.h`
- Modify: `src/module/eventloop.c`
- Modify: `tests/unit/test_eventloop.c`

- [x] **Step 1: Declare `cobalt_eventloop_post` in `include/cobalt/module/eventloop.h`**

```c
typedef void (*cobalt_loop_task_fn_t)(void *arg);

int cobalt_eventloop_post(cobalt_eventloop_t *loop, cobalt_loop_task_fn_t fn, void *arg);
```

- [x] **Step 2: Implement post queue and kernel wakeup in `src/module/eventloop.c`**

Implement:
- Initialize `cobalt_lfqueue_t *post_queue` using `cobalt_lfqueue_create_with_allocator`.
- Initialize `eventfd` on Linux (`EFD_NONBLOCK | EFD_CLOEXEC`) or self-pipe on macOS/POSIX. Register with `EPOLLIN`.
- Implement `cobalt_eventloop_post`: enqueue task, check `atomic_exchange(&loop->is_sleeping, false)`, write 8-byte counter to `wakeup_fd` if loop was sleeping.
- Handle wakeup event in `cobalt_eventloop_iteration`: drain `wakeup_fd`, dequeue and execute all posted tasks.
- Clean up queue and wakeup fd in `cobalt_eventloop_destroy`.

- [x] **Step 3: Add multi-threaded producer test in `tests/unit/test_eventloop.c`**

Spawn 4 worker threads submitting 20,000 tasks total via `cobalt_eventloop_post` while eventloop is running. Verify atomic counter reaches 20,000 with zero loss.

- [x] **Step 4: Build, verify tests & commit**

```bash
clang-format -i include/cobalt/module/eventloop.h src/module/eventloop.c tests/unit/test_eventloop.c
cmake --build build -j4 && ./build/tests/cobalt_test --filter eventloop
git add include/cobalt/module/eventloop.h src/module/eventloop.c tests/unit/test_eventloop.c
git commit -m "feat(eventloop): ✨ implement cross-thread post and eventfd wakeup"
```

---

## Task 4: High-Level Asynchronous Stream Abstraction (`cobalt_stream_t`)

**Files:**
- Modify: `include/cobalt/module/eventloop.h`
- Modify: `src/module/eventloop.c`
- Modify: `tests/unit/test_eventloop.c`

- [ ] **Step 1: Declare `cobalt_stream_t` in `include/cobalt/module/eventloop.h`**

```c
#include "cobalt/container/buffer.h"

typedef struct cobalt_stream cobalt_stream_t;

typedef void (*cobalt_stream_read_fn)(cobalt_stream_t *stream, cobalt_buffer_t *read_buf, void *user_data);
typedef void (*cobalt_stream_write_complete_fn)(cobalt_stream_t *stream, void *user_data);
typedef void (*cobalt_stream_error_fn)(cobalt_stream_t *stream, int err_code, void *user_data);

cobalt_stream_t *cobalt_stream_create(cobalt_eventloop_t *loop, cobalt_fd_t fd, void *user_data);
void             cobalt_stream_destroy(cobalt_stream_t *stream);

void             cobalt_stream_set_callbacks(cobalt_stream_t                *stream,
                                             cobalt_stream_read_fn           on_read,
                                             cobalt_stream_write_complete_fn on_write_complete,
                                             cobalt_stream_error_fn          on_error);

int              cobalt_stream_send(cobalt_stream_t *stream, const void *data, size_t len);
int              cobalt_stream_send_buffer(cobalt_stream_t *stream, cobalt_buffer_t *buf);

cobalt_buffer_t *cobalt_stream_get_input_buffer(cobalt_stream_t *stream);
cobalt_buffer_t *cobalt_stream_get_output_buffer(cobalt_stream_t *stream);
```

- [ ] **Step 2: Implement `cobalt_stream_t` in `src/module/eventloop.c`**

Implement stream lifecycle, direct write fast-path, automatic `POLLOUT` toggle on partial writes, and read pipeline using `cobalt_buffer_read_fd`.

- [ ] **Step 3: Add streaming echo tests in `tests/unit/test_eventloop.c`**

Create `socketpair`, stream 10MB across sender and receiver streams, verify data integrity, partial write buffering, and write completion callbacks.

- [ ] **Step 4: Build, verify tests & commit**

```bash
clang-format -i include/cobalt/module/eventloop.h src/module/eventloop.c tests/unit/test_eventloop.c
cmake --build build -j4 && ./build/tests/cobalt_test --filter eventloop
git add include/cobalt/module/eventloop.h src/module/eventloop.c tests/unit/test_eventloop.c
git commit -m "feat(eventloop): ✨ implement cobalt_stream_t async buffered stream"
```

---

## Task 5: Dedicated EventLoop & Buffer Benchmark Suite

**Files:**
- Create: `tests/benchmark/benchmark_eventloop.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/benchmark/bench_runner.c`

- [ ] **Step 1: Create `tests/benchmark/benchmark_eventloop.c`**

Benchmark:
1. Timing Wheel throughput (100,000 adds, ticks, and cancels).
2. Cross-thread task post throughput (Million tasks/sec).
3. Zero-copy buffer throughput (GB/sec append and retrieve).
Print `"Benchmark completed\n"`.

- [ ] **Step 2: Wire into `tests/CMakeLists.txt` and `tests/benchmark/bench_runner.c`**

Register `benchmark_eventloop` executable and `bench_eventloop` test in `tests/CMakeLists.txt`. Wire into `bench_runner.c` in both human-readable and `--json` modes.

- [ ] **Step 3: Build, run benchmarks & commit**

```bash
clang-format -i tests/benchmark/benchmark_eventloop.c tests/benchmark/bench_runner.c
cmake --build build -j4
./build/tests/benchmark_eventloop
ctest --test-dir build --output-on-failure -R bench_eventloop
git add tests/benchmark/benchmark_eventloop.c tests/CMakeLists.txt tests/benchmark/bench_runner.c
git commit -m "test(bench): ⚡ add benchmark suite for event loop and buffer engine"
```

---

## Task 6: Full Test Suite, Sanitizer Check & Format Gate

**Files:**
- Repository-wide verification across all modified and created files.

- [ ] **Step 1: Clang-format verification**

```bash
clang-format -i include/cobalt/container/buffer.h src/container/buffer.c tests/unit/test_buffer.c include/cobalt/module/eventloop.h src/module/eventloop.c tests/unit/test_eventloop.c tests/benchmark/benchmark_eventloop.c
clang-format --dry-run -Werror include/cobalt/container/buffer.h src/container/buffer.c tests/unit/test_buffer.c include/cobalt/module/eventloop.h src/module/eventloop.c tests/unit/test_eventloop.c tests/benchmark/benchmark_eventloop.c
```

- [ ] **Step 2: Clang-tidy static analysis**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCOBALT_RUN_TIDY=ON
cmake --build build -j4
```

- [ ] **Step 3: Full CTest execution**

```bash
ctest --test-dir build --output-on-failure
```

- [ ] **Step 4: AddressSanitizer & LeakSanitizer execution**

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCOBALT_ENABLE_ASAN=ON
cmake --build build-asan -j4
ctest --test-dir build-asan --output-on-failure
```

- [ ] **Step 5: Commit finalization**

```bash
git add -f docs/superpowers/plans/2026-10-17-eventloop.md
git commit -m "chore(eventloop): 🧹 format codebase and finalize Phase 3 eventloop and buffer engine" --allow-empty
```
