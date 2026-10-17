# API Reference — Module Layer (Layer 1-2)

**Modules:**
- JSON Processing (`include/cobalt/module/json.h`)
- Event Loop Engine (`include/cobalt/module/eventloop.h`)
- Work-Stealing Thread Pool (`include/cobalt/module/threadpool.h`)

---

## 1. JSON Module

**Header:** `include/cobalt/module/json.h`

### Data Types

```c
typedef enum {
    JSON_NULL = 0,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} json_type_t;

typedef union {
    double number;
    char  *string;
} json_value_t;

struct json_node {
    json_type_t type;
    json_value_t value;
    struct json_node *next;  /* next element (array) or next KV pair (object) */
    char *key;               /* object key, NULL for arrays/root */
};
```

### Core Operations

```c
json_node_t *json_parse(const char *text);
char        *json_serialize(json_node_t *node);
void         json_destroy(json_node_t *node);

/* Allocator-injectable variants */
json_node_t *json_parse_with_alloc(const char *text, cobalt_allocator_t *alloc);
char        *json_serialize_with_alloc(json_node_t *node, cobalt_allocator_t *alloc);
void         json_destroy_with_alloc(json_node_t *node, cobalt_allocator_t *alloc);
```

| Function | Behavior |
|:---|:---|
| `json_parse` | Parses JSON string into a node tree using system allocator. Returns NULL on syntax error. |
| `json_parse_with_alloc` | Parses JSON string using caller-supplied allocator. |
| `json_serialize` | Serializes node tree into a heap-allocated JSON string (caller must `free`). |
| `json_serialize_with_alloc` | Serializes node tree; returned buffer must be freed with `alloc->free`. |
| `json_destroy` | Recursively frees all nodes, strings, and keys. Safe to pass NULL. |

### Navigation & Value Accessors

```c
json_node_t *json_tree_get_child(json_node_t *parent, const char *key);
double       json_get_number(json_node_t *node);
const char  *json_get_string(json_node_t *node);
int          json_is_null(json_node_t *node);
int          json_is_object(json_node_t *node);
int          json_is_array(json_node_t *node);
```

---

## 2. Event Loop Engine

**Header:** `include/cobalt/module/eventloop.h`

### Types & Callbacks

```c
typedef int      cobalt_fd_t;          // File descriptor
typedef short    cobalt_events_t;      // Event mask (POLLIN, POLLOUT, etc.)
typedef uint64_t cobalt_timeout_ms_t;  // Delay in milliseconds
typedef uint64_t cobalt_interval_ms_t; // Periodic interval in milliseconds

typedef void (*fd_handler_t)(cobalt_fd_t fd, cobalt_events_t events, void *user_data);
typedef void (*timer_handler_t)(uint64_t timer_id, void *user_data);
typedef void (*cobalt_loop_task_fn_t)(void *arg);

typedef struct cobalt_eventloop cobalt_eventloop_t;
```

### Loop Lifecycle

```c
cobalt_eventloop_t *cobalt_eventloop_create(void);
void                cobalt_eventloop_destroy(cobalt_eventloop_t *loop);
void                cobalt_eventloop_run(cobalt_eventloop_t *loop);
void                cobalt_eventloop_stop(cobalt_eventloop_t *loop);
int                 cobalt_eventloop_iteration(cobalt_eventloop_t *loop);
```

| Function | Description |
|:---|:---|
| `cobalt_eventloop_create` | Creates a new event loop. Backed by `epoll` on Linux, `kqueue` on macOS/BSD. Returns NULL on failure. |
| `cobalt_eventloop_destroy` | Cleans up registered timers, descriptors, and destroys the loop. |
| `cobalt_eventloop_run` | Blocks caller thread, continuously dispatching ready I/O and timers until `cobalt_eventloop_stop` is called. |
| `cobalt_eventloop_stop` | Signals running loop to terminate. Safe to call from signal handlers, timer callbacks, or other threads. |
| `cobalt_eventloop_iteration` | Non-blocking or short-timeout single pass. Processes currently ready events and expired timers. |

### Descriptor Event Registration

```c
int cobalt_eventloop_add_fd(cobalt_eventloop_t *loop, cobalt_fd_t fd, cobalt_events_t events, fd_handler_t callback, void *user_data);
int cobalt_eventloop_mod_fd(cobalt_eventloop_t *loop, cobalt_fd_t fd, cobalt_events_t events, fd_handler_t callback, void *user_data);
int cobalt_eventloop_del_fd(cobalt_eventloop_t *loop, cobalt_fd_t fd);
```

### 4-Level Timing Wheel

```c
uint64_t cobalt_eventloop_add_timer(cobalt_eventloop_t *loop,
                                    cobalt_timeout_ms_t timeout_ms,
                                    cobalt_interval_ms_t interval_ms,
                                    timer_handler_t callback,
                                    void *user_data);
int cobalt_eventloop_del_timer(cobalt_eventloop_t *loop, uint64_t timer_id);
```

- **Resolution & Range**: 1ms base tick across 4 wheels (256 + 64 + 64 + 64 slots), spanning up to ~18.64 hours without overflow.
- **$O(1)$ Cancellation**: Timers are indexed in an internal Thomas Wang 64-bit integer hash table; deletion takes strictly $O(1)$ time.
- **`timeout_ms`**: Delay before first trigger (0 = immediate expiration on next tick).
- **`interval_ms`**: Repeat interval (0 = one-shot timer, > 0 = recurring periodic timer).

### Cross-Thread Task Dispatch (`cobalt_eventloop_post`)

```c
int cobalt_eventloop_post(cobalt_eventloop_t *loop, cobalt_loop_task_fn_t fn, void *arg);
```

- **Thread-Safety**: Concurrently callable from any thread at any time.
- **Syscall Coalescing**: Uses an `atomic_bool is_sleeping` flag. An `eventfd` write is performed only if the loop is currently asleep in kernel polling (`epoll_wait`/`kevent`). If already awake, the syscall is skipped, achieving up to ~2,000,000 tasks/sec.

### Asynchronous Stream (`cobalt_stream_t`)

```c
typedef struct cobalt_stream cobalt_stream_t;

typedef void (*cobalt_stream_read_fn)(cobalt_stream_t *stream, cobalt_buffer_t *input_buf, void *user_data);
typedef void (*cobalt_stream_write_complete_fn)(cobalt_stream_t *stream, void *user_data);
typedef void (*cobalt_stream_error_fn)(cobalt_stream_t *stream, int err_code, void *user_data);

cobalt_stream_t *cobalt_stream_create(cobalt_eventloop_t *loop, cobalt_fd_t fd, void *user_data);
void             cobalt_stream_destroy(cobalt_stream_t *stream);
void             cobalt_stream_set_callbacks(cobalt_stream_t *stream,
                                             cobalt_stream_read_fn on_read,
                                             cobalt_stream_write_complete_fn on_write_complete,
                                             cobalt_stream_error_fn on_error);
int              cobalt_stream_send(cobalt_stream_t *stream, const void *data, size_t len);
int              cobalt_stream_send_buffer(cobalt_stream_t *stream, cobalt_buffer_t *buf);
cobalt_buffer_t *cobalt_stream_get_input_buffer(cobalt_stream_t *stream);
cobalt_buffer_t *cobalt_stream_get_output_buffer(cobalt_stream_t *stream);
```

- **Direct-Write Fast Path**: If the output buffer is empty, `cobalt_stream_send` attempts direct socket write.
- **Zero-Copy Remainder Buffering**: Any unwritten data automatically queues into the stream's internal output `cobalt_buffer_t`.
- **Automatic POLLOUT Toggling**: Automatically enables `POLLOUT` on partial writes and removes `POLLOUT` once the buffer drains, notifying `on_write_complete`.

### UNIX Sockets & Signals

```c
int cobalt_eventloop_create_unix_server(const char *path, cobalt_fd_t *sock_out);
cobalt_fd_t cobalt_eventloop_accept(cobalt_eventloop_t *loop, cobalt_fd_t server_fd);
int cobalt_eventloop_add_signal(cobalt_eventloop_t *loop, int signum, fd_handler_t callback, void *user_data);
```

---

## 3. Work-Stealing Thread Pool

**Header:** `include/cobalt/module/threadpool.h`

A lock-free task scheduler combining a global MPMC queue with per-worker Chase-Lev SPMC work-stealing deques.

### Configuration & Lifecycle

```c
typedef struct {
    size_t num_threads;        /**< Worker count (0 = auto-detect CPU cores) */
    size_t local_queue_size;   /**< Per-worker deque capacity (default 1024, power of 2) */
    size_t global_queue_size;  /**< Global MPMC queue capacity (default 65536, power of 2) */
    cobalt_allocator_t *alloc; /**< Memory allocator (NULL = system allocator) */
} cobalt_threadpool_config_t;

cobalt_threadpool_t *cobalt_threadpool_create(size_t num_threads);
cobalt_threadpool_t *cobalt_threadpool_create_with_config(const cobalt_threadpool_config_t *config);
void                 cobalt_threadpool_destroy(cobalt_threadpool_t *pool);
```

| Function | Description |
|:---|:---|
| `cobalt_threadpool_create(N)` | Creates thread pool with $N$ worker threads. Passing $0$ auto-detects system CPU core count. |
| `cobalt_threadpool_create_with_config` | Creates pool with custom queue sizes and allocator. |
| `cobalt_threadpool_destroy` | Shuts down workers gracefully, draining all pending tasks before freeing resources. |

### Task Dispatch & Parallel Loops

```c
typedef void (*cobalt_task_fn_t)(void *arg);
typedef void (*cobalt_parallel_for_fn_t)(size_t index, void *arg);

int  cobalt_threadpool_submit(cobalt_threadpool_t *pool, cobalt_task_fn_t fn, void *arg);
int  cobalt_threadpool_parallel_for(cobalt_threadpool_t *pool, size_t start, size_t end, size_t chunk_size, cobalt_parallel_for_fn_t fn, void *arg);
void cobalt_threadpool_wait_idle(cobalt_threadpool_t *pool);
```

- **`cobalt_threadpool_submit`**: Submits a closure. When called from a worker thread, pushes directly to the worker's local Chase-Lev deque with zero allocations. When called from an external thread, enqueues into the global lock-free MPMC queue.
- **`cobalt_threadpool_parallel_for`**: Partitions $[start, end)$ into chunks and executes in parallel. Calling thread assists in running chunks. Blocks until all iterations finish.
- **`cobalt_threadpool_wait_idle`**: Blocks until all queued and active tasks complete.

### Introspection

```c
size_t cobalt_threadpool_thread_count(const cobalt_threadpool_t *pool);
size_t cobalt_threadpool_active_tasks(const cobalt_threadpool_t *pool);
```

### WaitGroup Synchronization Barrier

```c
typedef struct cobalt_waitgroup cobalt_waitgroup_t;

cobalt_waitgroup_t *cobalt_waitgroup_create(void);
void                cobalt_waitgroup_add(cobalt_waitgroup_t *wg, int64_t delta);
void                cobalt_waitgroup_done(cobalt_waitgroup_t *wg);
void                cobalt_waitgroup_wait(cobalt_waitgroup_t *wg);
void                cobalt_waitgroup_destroy(cobalt_waitgroup_t *wg);
```

#### Help-First Work-Stealing Guarantee
When a worker thread calls `cobalt_waitgroup_wait(wg)`, it does **not** sleep on an OS condition variable. Instead, it enters an active execution loop, popping local tasks and stealing tasks from peer workers until the counter drops to zero. This completely prevents thread-starvation deadlocks in recursive fork-join task graphs.
