# API Reference — Module Layer (Layer 1-2)

**Modules:** JSON processing (`module/json.h`), Event loop (`module/eventloop.h`)

## JSON Module

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
```

| Function | Behavior |
|----------|----------|
| `json_parse` | Parses JSON text into a node tree. Returns NULL on parse failure. Caller must `json_destroy` the result. |
| `json_serialize` | Serializes node tree to JSON string. Returns heap-allocated string (caller must `free`). Returns `"{}"` on failure. |
| `json_destroy` | Recursively frees all nodes and their string values. Safe to call with NULL. |

### Custom Allocators

```c
json_node_t *json_parse_with_alloc(const char *text, cobalt_allocator_t *alloc);
char        *json_serialize_with_alloc(json_node_t *node, cobalt_allocator_t *alloc);
void         json_destroy_with_alloc(json_node_t *node, cobalt_allocator_t *alloc);
```

Passing NULL for `alloc` falls back to the system allocator, making the
`_with_alloc` variants drop-in replacements for the plain trio. A tree built by
`json_parse_with_alloc` MUST be freed with `json_destroy_with_alloc` using the
same allocator, and the string from `json_serialize_with_alloc` with that
allocator's `free` (not plain `free`).

**Usage example:**

```c
cobalt_allocator_t *alloc = cobalt_allocator_get_system();
json_node_t *root = json_parse_with_alloc("{\"a\": 1}", alloc);
char *s = json_serialize_with_alloc(root, alloc);
json_destroy_with_alloc(root, alloc);
cobalt_allocator_free(alloc, s);
```

### Navigation

```c
json_node_t *json_tree_get_child(json_node_t *parent, const char *key);
```
Finds child node by key in an object. Returns NULL if parent is not an object or key not found.

### Value Accessors

```c
double  json_get_number(json_node_t *node);
const char *json_get_string(json_node_t *node);
int   json_is_null(json_node_t *node);
int   json_is_object(json_node_t *node);
int   json_is_array(json_node_t *node);
```

**Usage example:**
```c
json_node_t *root = json_parse("{\"name\": \"Cobalt\", \"version\": 2}");
json_node_t *name_node = json_tree_get_child(root, "name");
const char *name = json_get_string(name_node);  // "Cobalt"

char *serialized = json_serialize(root);
json_destroy(root);
free(serialized);
```

## Event Loop Module

### Type Aliases

```c
typedef int      cobalt_fd_t;          // File descriptor
typedef short    cobalt_events_t;      // Event mask (POLLIN, POLLOUT, etc.)
typedef uint64_t cobalt_timeout_ms_t;  // Timeout in milliseconds
typedef uint64_t cobalt_interval_ms_t; // Timer interval in milliseconds
```

### Callback Types

```c
typedef void (*fd_handler_t)(cobalt_fd_t fd, cobalt_events_t events, void *user_data);
typedef void (*timer_handler_t)(uint64_t timer_id, void *user_data);
```

### Core API

```c
cobalt_eventloop_t *cobalt_eventloop_create(void);
void                cobalt_eventloop_destroy(cobalt_eventloop_t *loop);
int                 cobalt_eventloop_run(cobalt_eventloop_t *loop);
void                cobalt_eventloop_stop(cobalt_eventloop_t *loop);
int                 cobalt_eventloop_iteration(cobalt_eventloop_t *loop);
```

| Function | Behavior |
|----------|----------|
| `cobalt_eventloop_create` | Creates new event loop. Uses epoll on Linux, kqueue on macOS. Returns NULL on failure. |
| `cobalt_eventloop_destroy` | Destroys loop and all registered FDs/timers. |
| `cobalt_eventloop_run` | Blocks until `cobalt_eventloop_stop` is called. Processes ready events. |
| `cobalt_eventloop_stop` | Signals the loop to exit from another thread or timer callback. |
| `cobalt_eventloop_iteration` | Non-blocking single iteration. Returns 0 on success, -1 on error. |

### FD Event Management

```c
int cobalt_eventloop_add_fd(cobalt_eventloop_t *loop,
                            cobalt_fd_t fd,
                            cobalt_events_t events,
                            fd_handler_t callback,
                            void *user_data);

int cobalt_eventloop_mod_fd(cobalt_eventloop_t *loop,
                            cobalt_fd_t fd,
                            cobalt_events_t events,
                            fd_handler_t callback,
                            void *user_data);

int cobalt_eventloop_del_fd(cobalt_eventloop_t *loop, cobalt_fd_t fd);
```

| Function | Behavior |
|----------|----------|
| `cobalt_eventloop_add_fd` | Register FD for event monitoring. Events mask: `POLLIN`, `POLLOUT`, or both. |
| `cobalt_eventloop_mod_fd` | Update existing FD registration (events, callback, or user_data). |
| `cobalt_eventloop_del_fd` | Remove FD registration. No-op if FD not registered. |

### Timer Management

```c
uint64_t cobalt_eventloop_add_timer(cobalt_eventloop_t  *loop,
                                    cobalt_timeout_ms_t  timeout_ms,
                                    cobalt_interval_ms_t interval_ms,
                                    timer_handler_t      callback,
                                    void                *user_data);

int cobalt_eventloop_del_timer(cobalt_eventloop_t *loop, uint64_t timer_id);
```

| Parameter | Behavior |
|-----------|----------|
| `timeout_ms` | Delay before first trigger (0 = immediate) |
| `interval_ms` | Interval between triggers (0 = one-shot, >0 = periodic) |
| `timer_id` | Returned on success (>0), 0 on failure |

**Usage example:**
```c
cobalt_eventloop_t *loop = cobalt_eventloop_create();

// Timer: print every second
cobalt_eventloop_add_timer(loop, 1000, 1000, timer_cb, NULL);

// FD: read from stdin
cobalt_eventloop_add_fd(loop, 0, POLLIN, fd_cb, NULL);

cobalt_eventloop_run(loop);
cobalt_eventloop_destroy(loop);
```

### Platform Backends

| Platform | Backend | Header |
|----------|---------|--------|
| Linux | epoll | `<sys/epoll.h>` |
| macOS | kqueue | `<sys/event.h>` |
| Windows | IOCP (planned) | — |

#
### UNIX Domain Sockets

```c
int cobalt_eventloop_create_unix_server(const char *path, cobalt_fd_t *sock_out);
int cobalt_eventloop_accept(cobalt_eventloop_t *loop, cobalt_fd_t sock_fd,
                            cobalt_event_cb_t callback, void *user_data,
                            int events);
```

| Function | Description |
|----------|-------------|
| `cobalt_eventloop_create_unix_server` | Create a UNIX domain socket server. Binds to `path`, listens, and registers for `POLLIN`. Automatically removes stale socket file. Returns 0 on success. |
| `cobalt_eventloop_accept` | Accept a pending connection on `sock_fd` and register it with the eventloop. Returns the new fd on success, -1 if no connection pending (non-blocking). |

**Usage pattern:**
```c
cobalt_fd_t server_fd;
cobalt_eventloop_create_unix_server("/tmp/cobalt.sock", &server_fd);

// In the server callback:
cobalt_fd_t client_fd = cobalt_eventloop_accept(loop, server_fd, client_handler, NULL, POLLIN);
if (client_fd >= 0) {
    // client_fd is now managed by the eventloop
}
```

## Error Handling

- All API functions set `COBALT_ERROR_OUT_OF_MEMORY` on allocation failure
- `cobalt_eventloop_add_fd` returns -1 for invalid FD or OOM
- `cobalt_eventloop_add_timer` returns 0 on failure
- Event loop is single-threaded — callbacks must not block
