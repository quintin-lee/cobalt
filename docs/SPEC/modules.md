# SPEC: Modules & Utilities (Layer 2)

**Module:** `include/cobalt/module/`, `src/module/`  
**Files:** json.h, eventloop.h, json.c, eventloop.c

## 1. Overview

Domain-level building blocks that sit atop the foundation layers. These modules solve common application problems using containers, algorithms, and object systems from lower layers.

## 2. JSON Module

**Header:** `module/json.h`

In-memory representation as a tree of `json_node_t` structs:

```c
typedef enum {
    JSON_NULL = 0, JSON_TRUE, JSON_FALSE,
    JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT
} json_type_t;

struct json_node {
    json_type_t type;
    union {
        double number;
        char *string;
    } value;
    struct json_node *next;   // Chaining for array/object children
    char *key;                // NULL for array items, key for object fields
};
```

API:

| Function | Description |
|----------|-------------|
| `json_parse(text)` | Parse JSON string into tree |
| `json_serialize(node)` | Convert tree back to JSON string |
| `json_destroy(node)` | Free tree and all children |
| `json_get_number(node)`, `json_get_string(node)` | Typed accessors |
| `json_is_null/is_object/is_array(node)` | Type predicates |

Usage pattern: parse incoming JSON → traverse tree → extract values → build Cobalt objects → respond via `json_serialize()`.

Memory: all strings allocated from the caller-provided allocator (or system default by default).

## 3. Event Loop Module

**Header:** `module/eventloop.h`

Single-threaded async I/O multiplexing with timer support. Backend uses `epoll` (Linux), `kqueue` (BSD/macOS), or `IOCP`/poll (Windows) transparently through the Platform layer.

Types:

```c
typedef void (*fd_handler_t)(int fd, short events, void *user_data);
typedef void (*timer_handler_t)(uint64_t id, void *user_data);

typedef struct cobalt_eventloop cobalt_eventloop_t;
```

API:

| Function | Description |
|----------|-------------|
| `eventloop_create()` | New event loop instance |
| `eventloop_destroy(loop)` | Free instance |
| `add_fd(loop, fd, events, cb, ctx)` | Register FD with callback |
| `mod_fd(loop, fd, events, cb, ctx)` | Modify existing FD handler |
| `del_fd(loop, fd)` | Remove FD registration |
| `add_timer(loop, timeout_ms, interval_ms, cb, ctx)` | Start recurring one-shot timer |
| `del_timer(loop, timer_id)` | Stop timer |
| `run(loop)` | Blocking main loop (process events until stop) |
| `stop(loop)` | Terminate running loop |
| `iteration(loop)` | Poll-ready non-blocking step |

Event types include readable/writable/POLLERR/POLLHUP semantics mapped to platform equivalents.

Timer IDs are monotonically increasing integers returned by add_timer; used for cancellation.

## 3.1 UNIX Domain Socket Support

**Header:** `module/eventloop.h`  
**Source:** `src/module/eventloop.c`

Unix domain sockets provide efficient local IPC (inter-process communication) on POSIX systems. They avoid network stack overhead and support passing file descriptors between processes.

```c
// Create a UNIX domain socket server
// Binds to path, listens, and registers for POLLIN
// Automatically removes stale socket file
int cobalt_eventloop_create_unix_server(const char *path, cobalt_fd_t *sock_out);

// Accept a connection on a UNIX domain socket server
// Returns a new fd ready for read/write, or -1 on error
cobalt_fd_t cobalt_eventloop_accept(cobalt_eventloop_t *loop, cobalt_fd_t server_fd);
```

**`cobalt_eventloop_create_unix_server`**: Creates a UNIX domain socket server. Binds to the given `path`, calls `listen()`, and registers the socket for `POLLIN` events on the event loop. Automatically removes any stale socket file at the path. Returns 0 on success.

**`cobalt_eventloop_accept`**: Accepts a pending connection on the server socket. Returns a new file descriptor ready for I/O operations, or -1 if no connection is pending or an error occurred.

Usage pattern:
```c
cobalt_fd_t server_fd;
cobalt_eventloop_create_unix_server("/tmp/cobalt.sock", &server_fd);

// Register accept callback on the event loop
eventloop_add_fd(loop, server_fd, POLLIN, accept_handler, ctx);

// In accept_handler:
cobalt_fd_t client_fd = cobalt_eventloop_accept(loop, server_fd);
eventloop_add_fd(loop, client_fd, POLLIN, client_handler, ctx);
```

Note: UNIX domain sockets are only available on POSIX systems (Linux, macOS, BSD). On Windows, use TCP localhost sockets instead.

## 4. Module Interaction Patterns

JSON parsers construct tree nodes using the allocator from the calling context (often an arena from a request-scoped allocation). Event loops integrate with the platform layer's IO abstraction, and callbacks may enqueue work into queues (containers) processed elsewhere.

## 5. Future Module Extensions (Planned)

- Binary serialization pack/unpack (more compact than JSON)
- Signal/slot publish-subscribe system
- TLS socket wrapper above event loop
- HTTP request/response framing module
