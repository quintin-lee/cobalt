# Architecture Design Specification: Phase 3 EventLoop Hierarchical Timing Wheel & Zero-Copy Async Buffer Engine

**Date:** 2026-10-17  
**Author:** Antigravity  
**Status:** Approved  
**Target:** Cobalt C11 Framework (`src/container/buffer.c`, `include/cobalt/container/buffer.h`, `src/module/eventloop.c`, `include/cobalt/module/eventloop.h`)

---

## 1. Executive Summary & Goals

Phase 3 upgrades Cobalt's networking and event-driven runtime from a basic epoll/poll wrapper with a binary timer heap into an industrial-grade, high-throughput asynchronous engine. It introduces two major decoupled subsystems:

1. **Layer 4 Reusable Zero-Copy Dynamic Buffer (`cobalt_buffer_t`)**:
   - A contiguous ring/slice buffer featuring prepended protocol header space, automated defragmentation, zero-copy slicing, and two-segment vectorized `readv` with a 64KB stack buffer.
2. **Layer 2 EventLoop Modernization (`cobalt_eventloop_t`)**:
   - **4-Level Hierarchical Timing Wheel**: Replaces the $O(\log N)$ insert / $O(N)$ cancel binary min-heap with a 4-level bitwise timing wheel (256 + 64 + 64 + 64 slots) providing strict $O(1)$ insertion, $O(1)$ cancellation via intrusive doubly-linked list nodes, and 1ms base resolution covering up to 18.64 hours in only ~3.5 KB memory footprint.
   - **Cross-Thread Asynchronous Wakeup & Task Dispatch (`cobalt_eventloop_post`)**: Integrates Layer 4 lock-free MPMC queue (`cobalt_lfqueue_t`) with non-blocking `eventfd` (Linux) / self-pipe (POSIX/macOS) and syscall coalescing (`atomic_bool is_sleeping`) to bridge Phase 2 ThreadPool worker threads and the EventLoop main thread.
   - **High-Level Asynchronous Stream Abstraction (`cobalt_stream_t`)**: Encapsulates partial writes, automatic `POLLOUT` subscription/unsubscription, and non-blocking stream reassembly on top of `cobalt_buffer_t`.
   - **100% Backward Compatibility**: All legacy `cobalt_eventloop_*` APIs remain fully supported without breaking existing callers.

---

## 2. Layering & Architectural Alignment

Cobalt enforces a strict upward-only dependency hierarchy. The Phase 3 subsystems respect this invariant:

```
+--------------------------------------------------------------------------+
|  Layer 2: Modules (src/module/eventloop.c, threadpool.c, json.c)         |
|  - cobalt_eventloop_t: epoll/kqueue/poll reactor, 4-level timing wheel   |
|  - cobalt_stream_t: async buffered stream abstraction                     |
|  - cobalt_eventloop_post: cross-thread task queue & eventfd wakeup       |
+--------------------------------------------------------------------------+
                                     |
                                     v Downward dependency
+--------------------------------------------------------------------------+
|  Layer 4: Containers (src/container/buffer.c, lfqueue.c, vector.c)       |
|  - cobalt_buffer_t: dynamic zero-copy byte buffer                        |
|  - cobalt_lfqueue_t: lock-free MPMC ring queue                           |
+--------------------------------------------------------------------------+
                                     |
                                     v Downward dependency
+--------------------------------------------------------------------------+
|  Layer 7 & 8: Memory Allocators & Platform OS Primitives                  |
|  - Layer 7a: allocator.h (cobalt_allocator_t)                            |
|  - Layer 7b: error.h (cobalt_error_t)                                    |
|  - Layer 8:  thread.h, atomic.h (stdatomic.h, C11 atomics)               |
+--------------------------------------------------------------------------+
```

---

## 3. Subsystem 1: Layer 4 Dynamic Zero-Copy Buffer (`cobalt_buffer_t`)

### 3.1 Memory Layout

The buffer maintains three contiguous regions bounded by two monotonic cursors (`read_pos` and `write_pos`):

```
+---------------------+-----------------------+---------------------+
|  Prependable Bytes  |   Readable Payload    |   Writable Bytes    |
| (Reserved for hdr)  |  (write_pos-read_pos) | (capacity-write_pos)|
+---------------------+-----------------------+---------------------+
0          <=      read_pos        <=      write_pos     <=     capacity
```

- **Prependable region** (default 8 bytes): Allows prepending framing headers (e.g. 4-byte packet length, 2-byte magic, 2-byte opcode) backwards in $O(1)$ without calling `memmove` on payload bytes.
- **Readable region**: Calling `cobalt_buffer_peek()` returns a direct read pointer to `data + read_pos` without memory allocation.
- **Writable region**: New data is appended at `data + write_pos`.
- **In-place defragmentation**: When writable space is insufficient but prepended/discarded bytes plus writable space can satisfy the demand, payload bytes are shifted back to `prepend_size` via a single `memmove`. If total space is still insufficient, buffer expands geometrically (1.5x/2x).

### 3.2 Data Structure & API

Located in `include/cobalt/container/buffer.h`:

```c
typedef struct cobalt_buffer {
    char               *data;
    size_t              capacity;
    size_t              read_pos;
    size_t              write_pos;
    size_t              prepend_size;
    cobalt_allocator_t *alloc;
} cobalt_buffer_t;

/* Lifecycle */
cobalt_buffer_t *cobalt_buffer_create(size_t initial_capacity);
cobalt_buffer_t *cobalt_buffer_create_with_alloc(size_t initial_capacity,
                                                size_t prepend_size,
                                                cobalt_allocator_t *alloc);
void             cobalt_buffer_destroy(cobalt_buffer_t *buf);

/* State & Capacity Inspection */
size_t           cobalt_buffer_readable_bytes(const cobalt_buffer_t *buf);
size_t           cobalt_buffer_writable_bytes(const cobalt_buffer_t *buf);
size_t           cobalt_buffer_prependable_bytes(const cobalt_buffer_t *buf);
const char      *cobalt_buffer_peek(const cobalt_buffer_t *buf);

/* Reading & Consumption */
int              cobalt_buffer_retrieve(cobalt_buffer_t *buf, size_t len);
void             cobalt_buffer_retrieve_all(cobalt_buffer_t *buf);
int              cobalt_buffer_read(cobalt_buffer_t *buf, void *dst, size_t len);

/* Writing & Prepending */
int              cobalt_buffer_ensure_writable(cobalt_buffer_t *buf, size_t len);
int              cobalt_buffer_append(cobalt_buffer_t *buf, const void *data, size_t len);
int              cobalt_buffer_prepend(cobalt_buffer_t *buf, const void *data, size_t len);

/* Vectorized I/O with Stack Buffer */
ssize_t          cobalt_buffer_read_fd(cobalt_buffer_t *buf, int fd, int *err_out);
ssize_t          cobalt_buffer_write_fd(cobalt_buffer_t *buf, int fd, int *err_out);
```

### 3.3 Vectorized `readv` Two-Segment Stack Buffer Optimization

In `cobalt_buffer_read_fd`:
```c
struct iovec iov[2];
char stack_extra[65536]; /* 64KB stack buffer */

iov[0].iov_base = buf->data + buf->write_pos;
iov[0].iov_len  = writable;
iov[1].iov_base = stack_extra;
iov[1].iov_len  = sizeof(stack_extra);

ssize_t n = readv(fd, iov, writable < sizeof(stack_extra) ? 2 : 1);
if (n > (ssize_t)writable) {
    buf->write_pos = buf->capacity;
    cobalt_buffer_append(buf, stack_extra, (size_t)n - writable);
} else if (n > 0) {
    buf->write_pos += (size_t)n;
}
```
This guarantees reading up to 64KB+ in a single non-blocking syscall without pre-allocating oversized buffers for idle sockets.

---

## 4. Subsystem 2: Layer 2 4-Level Hierarchical Timing Wheel

### 4.1 Hierarchy Specification

The timing wheel uses bitwise masking and shifting across 4 levels with a 1 ms tick resolution:

| Level | Slot Count | Slot Granularity | Level Span | Bitwise Mask / Shift |
|---|---|---|---|---|
| **L0** | 256 ($2^8$) | 1 ms | 256 ms | `(time) & 0xFF` |
| **L1** | 64 ($2^6$) | 256 ms | ~16.38 s | `(time >> 8) & 0x3F` |
| **L2** | 64 ($2^6$) | ~16.38 s | ~17.47 min | `(time >> 14) & 0x3F` |
| **L3** | 64 ($2^6$) | ~17.47 min | **~18.64 hours** | `(time >> 20) & 0x3F` |

* **Total Slots**: $256 + 64 + 64 + 64 = 448$ list heads ($\sim$3.5 KB RAM, stays resident in L1 Data Cache).
* **Cascade**: When L0 completes a cycle (256 ticks), L1 advances 1 slot and cascades its list into L0. Higher levels cascade similarly.

### 4.2 Intrusive Node & $O(1)$ Operations

```c
typedef struct cobalt_timer_node {
    uint64_t                  timer_id;
    uint64_t                  expire_time_ms;
    uint64_t                  interval_ms;
    timer_handler_t           callback;
    void                     *user_data;
    struct cobalt_timer_node *prev;
    struct cobalt_timer_node *next;
    uint8_t                   level;
    uint8_t                   slot;
    bool                      active;
} cobalt_timer_node_t;
```

- **Insertion**: $O(1)$ constant time based on `delta = expire_time_ms - current_wheel_time`.
- **Cancellation**: $O(1)$ node unlink via intrusive doubly-linked list (`prev->next = next; next->prev = prev;`).
- **Epoll Timeout Optimization**: Scans Level 0 to find the nearest non-empty slot to compute exact milliseconds for `epoll_wait(..., timeout_ms)`.

---

## 5. Subsystem 3: Cross-Thread Async Wakeup & Post Queue

### 5.1 Architecture

```
[ Worker Thread 1 ] \
[ Worker Thread 2 ]  --> [ cobalt_lfqueue_t (L4 MPMC Queue) ] ---> [ EventLoop Loop Thread ]
[ External Thread ] /                 |                                       ^
                                      v if is_sleeping == true                |
                         [ Linux: eventfd / POSIX: self-pipe ] ---------------+ (Kernel Wakeup)
```

### 5.2 API & Implementation

```c
typedef void (*cobalt_loop_task_fn_t)(void *arg);

int cobalt_eventloop_post(cobalt_eventloop_t *loop, cobalt_loop_task_fn_t fn, void *arg);
```

1. **Queueing**: Pushes `(fn, arg)` into `loop->post_queue` (`cobalt_lfqueue_t`).
2. **Syscall Coalescing**:
   ```c
   if (atomic_exchange_explicit(&loop->is_sleeping, false, memory_order_seq_cst)) {
       uint64_t one = 1;
       write(loop->wakeup_fd, &one, sizeof(one));
   }
   ```
   Eliminates kernel write overhead when the loop thread is already actively running.
3. **Batch Drain**: On wakeup event, drains the post queue completely and executes tasks sequentially in the event loop thread without acquiring any mutex.

---

## 6. Subsystem 4: High-Level Asynchronous Stream Abstraction (`cobalt_stream_t`)

### 6.1 Data Structure & API

```c
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

### 6.2 Stream Flow Management

1. **Read Pipeline**: Reads via `cobalt_buffer_read_fd` into `input_buf`. Invokes `on_read()`. Emits `on_error(stream, 0, user_data)` on EOF.
2. **Send Pipeline**: If `output_buf` is empty, attempts immediate non-blocking write. If partial write occurs or socket returns `EAGAIN`, appends remainder to `output_buf` and registers `POLLOUT` with the event loop.
3. **Write Pipeline**: When `POLLOUT` fires, flushes `output_buf`. Once empty, unregisters `POLLOUT` and invokes `on_write_complete()`.

---

## 7. Verification & Benchmarking Strategy

### 7.1 Unit Tests

1. `tests/unit/test_buffer.c`:
   - Basic append/retrieve, `retrieve_all`.
   - Prependable space without copying content.
   - Defragmentation and exponential expansion.
   - Two-segment `readv` across pipes.
   - Allocator injection lifecycle balance.
2. `tests/unit/test_eventloop.c`:
   - 10,000 timers concurrently armed across 4 levels.
   - $O(1)$ random timer cancellation (cancel 5,000 mid-flight).
   - Multi-thread concurrent `post` (4 threads, 20,000 tasks) with atomic consistency.
   - `cobalt_stream_t` echo test streaming 10 MB across `socketpair`.

### 7.2 Benchmark Suite (`tests/benchmark/benchmark_eventloop.c`)

- Timing Wheel throughput vs legacy heap ($100,000$ operations).
- Cross-thread task post throughput (Million ops/sec).
- Zero-copy buffer throughput (GB/sec).
- Registration in `tests/CMakeLists.txt` and `bench_runner.c`.

### 7.3 Quality Gates

- `-Wall -Wextra -Werror` zero warnings.
- `clang-tidy` clean.
- `clang-format` 100% compliant.
- AddressSanitizer (ASan) & LeakSanitizer (LSan) 100% clean across all tests.

---

## 8. Git Commit Guidelines

Following AGENTS.md conventions:
- `feat(buffer): ✨ implement Layer 4 dynamic zero-copy buffer engine`
- `feat(eventloop): ✨ implement 4-level hierarchical timing wheel`
- `feat(eventloop): ✨ implement cross-thread post and eventfd wakeup`
- `feat(eventloop): ✨ implement cobalt_stream_t async buffered stream`
- `test(bench): ⚡ add benchmark suite for event loop and buffer engine`
- `chore(eventloop): 🧹 format codebase and finalize Phase 3`
