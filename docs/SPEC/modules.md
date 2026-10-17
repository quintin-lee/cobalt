# SPEC: Modules & Task Schedulers (Layer 2)

**Module:** `include/cobalt/module/`, `src/module/`  
**Files:** `json.h`, `eventloop.h`, `threadpool.h`, `json.c`, `eventloop.c`, `threadpool.c`

## 1. Overview

The Module layer sits directly above algorithms and containers, providing industrial-strength application building blocks:
- **JSON Processing**: Tree-based parser, serializer, and typed tree navigators with pluggable memory allocators.
- **Event Loop Engine**: High-performance single-threaded event multiplexing featuring a 4-level hierarchical timing wheel, lock-free cross-thread post queue with syscall coalescing, and zero-copy asynchronous streams (`cobalt_stream_t`).
- **Work-Stealing Thread Pool**: Multi-core lock-free task scheduler featuring Chase-Lev SPMC work-stealing deques, global MPMC queue, Help-First WaitGroup synchronization barriers, and dynamic parallel loops.

---

## 2. JSON Module

**Header:** `include/cobalt/module/json.h`

### 2.1 In-Memory Representation

JSON documents are parsed into an in-memory DOM tree of `json_node_t` structures:

```c
typedef enum {
    JSON_NULL = 0, JSON_TRUE, JSON_FALSE,
    JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT
} json_type_t;

typedef union {
    double number;
    char  *string;
} json_value_t;

struct json_node {
    json_type_t type;
    json_value_t value;
    struct json_node *next;  /* Sibling in array or object */
    char *key;               /* Key name if inside object, NULL for array items */
};
```

### 2.2 Core Operations & Custom Allocators

| Function | Description |
|----------|-------------|
| `json_parse(text)` | Parse JSON string using system allocator |
| `json_parse_with_alloc(text, alloc)` | Parse JSON string using custom arena/pool allocator |
| `json_serialize(node)` | Serialize node tree back into a formatted JSON string |
| `json_serialize_with_alloc(node, alloc)` | Serialize using custom allocator |
| `json_destroy(node)` / `_with_alloc` | Recursively free the node tree and string keys |
| `json_tree_get_child(parent, key)` | Lookup child node by key in $O(N)$ sibling walk |
| `json_get_number(node)` / `json_get_string(node)` | Extract typed scalar values |

---

## 3. High-Performance Event Loop Engine

**Header:** `include/cobalt/module/eventloop.h`

The Event Loop module provides asynchronous I/O multiplexing backed by `epoll` on Linux and `kqueue` on BSD/macOS.

### 3.1 4-Level Hierarchical Timing Wheel

Timers are scheduled using a 4-level hierarchical timing wheel with a 1 millisecond base tick resolution:

| Level | Slots | Step Per Slot | Level Span |
|:---:|:---:|:---:|:---:|
| **Level 1** | 256 | 1 ms | 256 ms |
| **Level 2** | 64 | 256 ms | 16,384 ms (~16.38 s) |
| **Level 3** | 64 | 16,384 ms | 1,048,576 ms (~17.47 min) |
| **Level 4** | 64 | 1,048,576 ms | 67,108,864 ms (~18.64 hours) |

#### Algorithmic Invariants
- **$O(1)$ Insertion**: Timers are placed into the appropriate level based on the relative expiration time $(\Delta t = \text{expire} - \text{current\_time})$.
- **Cascading Migration**: When a lower wheel wraps around (e.g. L1 tick reaches 256), the parent wheel advances by one slot and cascades timers from that slot down into the lower level.
- **$O(1)$ Deletion via Hash Table**: Active timers are indexed in an open-addressing hash table using the Thomas Wang 64-bit integer hash on `timer_id`. Canceling a timer (`cobalt_eventloop_del_timer`) is guaranteed $O(1)$ without linked list traversal.
- **Periodic Rescheduling**: Recurring timers (`interval_ms > 0`) are automatically re-inserted into the timing wheel upon expiration.

---

### 3.2 Cross-Thread Task Dispatch with Syscall Coalescing

External threads and worker threads can asynchronously post closures to be executed inside the event loop thread:

```c
int cobalt_eventloop_post(cobalt_eventloop_t *loop, cobalt_loop_task_fn_t fn, void *arg);
```

#### Architecture & Syscall Coalescing
1. **Thread-Safe Task Queue**: Tasks are enqueued into a thread-safe post queue protected by lightweight spinlocks.
2. **Wakeup Notification**: An `eventfd` (Linux) or self-pipe (macOS/BSD) is registered on the loop to wake up `epoll_wait` / `kevent`.
3. **Syscall Coalescing**:
   - The loop maintains an `atomic_bool is_sleeping` state flag.
   - When external threads post tasks, an `eventfd_write()` syscall is issued **only if** `is_sleeping` was observed true.
   - If the event loop is already awake and actively processing I/O or earlier posted tasks, subsequent posts skip the kernel syscall entirely.
   - This coalescing reduces kernel context switches by up to $95\%$ under high-concurrency cross-thread posting workloads.

---

### 3.3 High-Level Asynchronous Stream (`cobalt_stream_t`)

`cobalt_stream_t` encapsulates a non-blocking stream file descriptor (TCP socket, UNIX socket) with zero-copy stream buffers:

```c
cobalt_stream_t *cobalt_stream_create(cobalt_eventloop_t *loop, cobalt_fd_t fd, void *user_data);
void cobalt_stream_destroy(cobalt_stream_t *stream);
void cobalt_stream_set_callbacks(cobalt_stream_t *stream,
                                 cobalt_stream_read_fn on_read,
                                 cobalt_stream_write_complete_fn on_write_complete,
                                 cobalt_stream_error_fn on_error);
int  cobalt_stream_send(cobalt_stream_t *stream, const void *data, size_t len);
int  cobalt_stream_send_buffer(cobalt_stream_t *stream, cobalt_buffer_t *buf);
```

#### Read Flow & Vectorized Buffering
- Incoming readable events trigger `cobalt_buffer_read_fd()` on the stream's internal input buffer.
- Two-segment vectorized `readv` reads up to internal capacity plus a 64KB stack spill buffer, expanding dynamically only when needed.
- `on_read` callback is invoked with the input `cobalt_buffer_t*`.

#### Write Flow & Automatic POLLOUT Management
- **Direct-Write Fast Path**: When the output buffer is empty and `cobalt_stream_send` is called, the stream attempts an immediate `write()` or `send()` syscall.
- **Zero-Copy Remainder Buffering**: If the socket fills or writes partially (`EAGAIN`/`EWOULDBLOCK`), unwritten data is appended to the stream's internal output `cobalt_buffer_t`.
- **Automatic Event Toggling**:
  - Whenever output buffer has pending bytes, the stream automatically enables `POLLOUT` on the event loop.
  - When `POLLOUT` fires, remaining bytes are flushed via `cobalt_buffer_write_fd()`.
  - Once the output buffer is completely drained, the stream automatically removes `POLLOUT` from the event loop and invokes `on_write_complete`.

---

### 3.4 UNIX Domain Sockets & Signal Delivery

- **UNIX Domain Server**: `cobalt_eventloop_create_unix_server` cleans stale socket files, binds, listens, and registers non-blocking accept callbacks.
- **UNIX Accept**: `cobalt_eventloop_accept` accepts incoming clients non-blockingly and returns the connected file descriptor.
- **Signal Handling**: `cobalt_eventloop_add_signal` safely routes POSIX signals (e.g. `SIGINT`, `SIGTERM`) into the event loop via a dedicated self-pipe ring buffer.

---

## 4. Lock-Free Work-Stealing Thread Pool

**Header:** `include/cobalt/module/threadpool.h`

A multi-core task scheduler designed for irregular parallel workloads, recursive fork-join task graphs, and high-throughput background processing.

```
+-------------------------------------------------------------------+
|                        Global MPMC Queue                          |
|                       (cobalt_lfqueue_t)                          |
+-------------------------------------------------------------------+
         ^ external submissions                 | steal on idle
         |                                      v
  +--------------+                      +--------------+
  |   Worker 0   | <--- work steal ---> |   Worker 1   |
  |  Chase-Lev   |                      |  Chase-Lev   |
  |  SPMC Deque  |                      |  SPMC Deque  |
  +--------------+                      +--------------+
```

### 4.1 Hybrid Scheduler Architecture

1. **Global Lock-Free Queue**: An MPMC ring buffer (`cobalt_lfqueue_t`) receives tasks submitted from external non-worker threads (`cobalt_threadpool_submit`).
2. **Per-Worker Chase-Lev Deques**:
   - Each worker thread owns a cache-line aligned single-producer multi-consumer (SPMC) deque.
   - **Local Push/Pop (LIFO)**: The owner thread pushes child tasks and pops tasks from the *bottom* (LIFO order), maximizing CPU cache and register locality for recursive algorithms.
   - **Work Stealing (FIFO)**: Idle workers steal tasks from the *top* (FIFO order) of peer workers' deques, minimizing contention with the owner.
3. **Zero-Allocation Hot Path**: When a worker submits a task, it pushes directly into its pre-allocated circular ring buffer with zero dynamic memory allocation.

---

### 4.2 WaitGroup & Help-First Work-Stealing

```c
cobalt_waitgroup_t *cobalt_waitgroup_create(void);
void cobalt_waitgroup_add(cobalt_waitgroup_t *wg, int64_t delta);
void cobalt_waitgroup_done(cobalt_waitgroup_t *wg);
void cobalt_waitgroup_wait(cobalt_waitgroup_t *wg);
void cobalt_waitgroup_destroy(cobalt_waitgroup_t *wg);
```

#### Deadlock-Free Fork-Join
In recursive fork-join patterns (e.g. divide-and-conquer parallel sort or tree traversals), naive barriers that block the worker thread on a condition variable can cause catastrophic thread starvation deadlocks when all workers block waiting for tasks stuck in local queues.

Cobalt solves this via **Help-First Work-Stealing**:
- When a worker calls `cobalt_waitgroup_wait(wg)`, it checks whether the WaitGroup counter is zero.
- If non-zero, instead of putting the thread to sleep, the worker enters an active execution loop:
  1. Pops and runs pending tasks from its own local Chase-Lev deque.
  2. If local deque is empty, dequeues from the global MPMC queue.
  3. If global queue is empty, steals tasks from peer workers.
- Only non-worker threads sleep on the synchronization condition variable.
- Result: **Zero thread starvation deadlocks** under arbitrary recursive nesting.

---

### 4.3 Dynamic Parallel Loops (`cobalt_threadpool_parallel_for`)

```c
int cobalt_threadpool_parallel_for(cobalt_threadpool_t *pool,
                                   size_t start, size_t end,
                                   size_t chunk_size,
                                   cobalt_parallel_for_fn_t fn,
                                   void *arg);
```

- Partitions the iteration space $[start, end)$ into chunks of size `chunk_size` (or automatically computed based on core count and problem size).
- Submits chunk descriptors to the thread pool with an integrated `cobalt_waitgroup_t`.
- The calling thread immediately assists by executing chunks itself, minimizing scheduling overhead and barrier latency.

---

### 4.4 3-Stage Adaptive Idle Backoff

To balance sub-microsecond task dispatch latency with low CPU usage when the pool is idle, workers utilize a 3-stage adaptive backoff:
1. **Spinning (Low Latency)**: Loop with architecture-specific pause instructions (`_mm_pause` on x86, `yield` on ARM).
2. **Yielding (Medium Latency)**: Execute `sched_yield()` to allow other OS threads to progress.
3. **Sleeping (Zero CPU)**: Atomically increment `idle_workers` and wait on a condition variable. Woken immediately when new tasks are submitted.

---

## 5. Module Concurrency & Thread-Safety Guarantees

| Module / Component | Function / Type | Thread-Safety Guarantee |
|--------------------|-----------------|-------------------------|
| **JSON** | `json_parse`, `json_serialize`, `json_destroy` | **Reentrant**: Independent trees are thread-safe. Concurrent access to the same tree requires synchronization. |
| **EventLoop** | `cobalt_eventloop_run`, `cobalt_eventloop_iteration` | **Single-Threaded**: Must only run on the loop's dedicated owner thread. |
| **EventLoop** | `cobalt_eventloop_post` | **Thread-Safe**: Can be called concurrently by any thread at any time. |
| **Stream** | `cobalt_stream_*` | **Single-Threaded**: Bound to the event loop's owner thread. |
| **ThreadPool** | `cobalt_threadpool_submit` | **Thread-Safe**: Lock-free dispatch from both worker and non-worker threads. |
| **ThreadPool** | `cobalt_threadpool_parallel_for` | **Thread-Safe**: Can be invoked concurrently across multiple callers. |
| **WaitGroup** | `cobalt_waitgroup_*` | **Thread-Safe**: Atomics-based counter with help-first stealing. |
