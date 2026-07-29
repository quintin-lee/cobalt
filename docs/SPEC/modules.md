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

## 4. Module Interaction Patterns

JSON parsers construct tree nodes using the allocator from the calling context (often an arena from a request-scoped allocation). Event loops integrate with the platform layer's IO abstraction, and callbacks may enqueue work into queues (containers) processed elsewhere.

## 5. Future Module Extensions (Planned)

- Binary serialization pack/unpack (more compact than JSON)
- Signal/slot publish-subscribe system
- TLS socket wrapper above event loop
- HTTP request/response framing module
