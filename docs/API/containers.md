# API Reference — Container Module (Layer 4)

**Module:** `include/cobalt/container/`  
**Dependencies:** `interface/sequence.h`, `interface/map.h`, `interface/iterator.h`

## Overview

The Container module provides 10 industrial-grade data structures. Sequential and associative containers store `void *` pointers without taking data ownership. The stream buffer (`cobalt_buffer_t`) provides dynamic contiguous byte storage, and the lock-free queue (`cobalt_lfqueue_t`) provides ultra-high-throughput concurrent pointer exchange.

| Container | Type | Time Complexity | Notes |
|:---|:---|:---:|:---|
| **Vector** | Dynamic array | $O(1)$ push/get, $O(N)$ insert/remove | 2x growth strategy, contiguous memory |
| **List** | Doubly-linked | $O(1)$ push/pop ends, $O(N)$ access | Bidirectional traversal, node stability |
| **Stack** | LIFO | $O(1)$ push/pop/peek | Singly-linked list |
| **Queue** | FIFO | $O(1)$ enqueue/dequeue/peek | Singly-linked list with tail pointer |
| **Deque** | Double-ended | $O(1)$ push/pop both ends | Doubly-linked list |
| **HashMap** | Hash table | $O(1)$ avg put/get/remove | Separate chaining, auto-rehash at 75% |
| **TreeMap** | Red-Black Tree | $O(\log N)$ put/get/remove | Self-balancing BST, sorted key traversal |
| **Set** | Hash set | $O(1)$ avg insert/contains/remove | Unique items, backed by hash table |
| **Buffer** | Stream buffer | $O(1)$ append/prepend/peek | 3-zone sliding layout, vectorized `readv` |
| **LFQueue** | Lock-free queue | $O(1)$ lock-free enqueue/dequeue | Bounded MPMC, 64-byte cache alignment |

---

## 1. Zero-Copy Stream Buffer (`cobalt_buffer_t`)

**Header:** `include/cobalt/container/buffer.h`

```c
#define COBALT_BUFFER_DEFAULT_INITIAL_CAPACITY 1024
#define COBALT_BUFFER_DEFAULT_PREPEND_SIZE 8

typedef struct cobalt_buffer cobalt_buffer_t;
```

### Lifecycle

```c
cobalt_buffer_t *cobalt_buffer_create(size_t initial_capacity);
cobalt_buffer_t *cobalt_buffer_create_with_alloc(size_t initial_capacity, size_t prepend_size, cobalt_allocator_t *alloc);
void             cobalt_buffer_destroy(cobalt_buffer_t *buf);
```

| Function | Description |
|:---|:---|
| `cobalt_buffer_create(cap)` | Allocates buffer with `cap` writable bytes (0 = 1024) and 8-byte prepend space using system allocator. |
| `cobalt_buffer_create_with_alloc` | Allocates buffer with custom prepend size and allocator. |
| `cobalt_buffer_destroy(buf)` | Frees buffer and internal memory. Safe with NULL. |

### Capacity & Inspection

```c
size_t      cobalt_buffer_readable_bytes(const cobalt_buffer_t *buf);
size_t      cobalt_buffer_writable_bytes(const cobalt_buffer_t *buf);
size_t      cobalt_buffer_prependable_bytes(const cobalt_buffer_t *buf);
const char *cobalt_buffer_peek(const cobalt_buffer_t *buf);
```

- **`cobalt_buffer_readable_bytes`**: Returns count of unread payload bytes ($write\_pos - read\_pos$).
- **`cobalt_buffer_writable_bytes`**: Returns count of free writable bytes without resizing ($capacity - write\_pos$).
- **`cobalt_buffer_prependable_bytes`**: Returns count of bytes reserved before $read\_pos$.
- **`cobalt_buffer_peek`**: Direct read-only pointer to readable payload cursor. Returns NULL if empty.

### Reading & Consuming

```c
int  cobalt_buffer_retrieve(cobalt_buffer_t *buf, size_t len);
void cobalt_buffer_retrieve_all(cobalt_buffer_t *buf);
int  cobalt_buffer_read(cobalt_buffer_t *buf, void *dst, size_t len);
```

- **`cobalt_buffer_retrieve`**: Consumes and advances read cursor by `len` bytes without copying. Returns 0 on success, -1 if `len > readable`.
- **`cobalt_buffer_retrieve_all`**: Resets `read_pos` and `write_pos` to initial prepend offset, retaining buffer allocation.
- **`cobalt_buffer_read`**: Copies `len` bytes into `dst` and advances read cursor.

### Appending & Prepending

```c
int cobalt_buffer_ensure_writable(cobalt_buffer_t *buf, size_t len);
int cobalt_buffer_append(cobalt_buffer_t *buf, const void *data, size_t len);
int cobalt_buffer_prepend(cobalt_buffer_t *buf, const void *data, size_t len);
```

- **`cobalt_buffer_ensure_writable`**: Guarantees at least `len` writable bytes. Performs in-place defragmentation (compacting readable bytes left) if possible; expands buffer dynamically only if capacity is insufficient.
- **`cobalt_buffer_append`**: Appends bytes to the end of readable payload.
- **`cobalt_buffer_prepend`**: Writes header bytes directly into the prependable zone before `read_pos`. Returns 0 on success, -1 if `len > prependable`.

### Vectorized Socket I/O

```c
ssize_t cobalt_buffer_read_fd(cobalt_buffer_t *buf, int fd, int *out_err);
ssize_t cobalt_buffer_write_fd(cobalt_buffer_t *buf, int fd, int *out_err);
```

- **`cobalt_buffer_read_fd`**: Uses two-segment `readv` (internal writable slice + 64KB stack buffer) to read maximum available socket data non-blockingly with zero unnecessary allocations. Returns bytes read, or -1 on error.
- **`cobalt_buffer_write_fd`**: Writes readable bytes directly to non-blocking descriptor and advances read cursor. Returns bytes written, or -1 on error.

---

## 2. Lock-Free Bounded MPMC Queue (`cobalt_lfqueue_t`)

**Header:** `include/cobalt/container/lfqueue.h`

```c
typedef struct cobalt_lfqueue cobalt_lfqueue_t;
```

### Lifecycle

```c
cobalt_lfqueue_t *cobalt_lfqueue_create(size_t capacity);
cobalt_lfqueue_t *cobalt_lfqueue_create_with_allocator(size_t capacity, cobalt_allocator_t *alloc);
void              cobalt_lfqueue_destroy(cobalt_lfqueue_t *queue);
```

- Capacity is automatically rounded up to the nearest power of two ($2^k$, minimum 2).
- Internal producer and consumer cursors are 64-byte aligned (`_Alignas(64)`) to eliminate false sharing.

### Non-Blocking Operations

```c
int    cobalt_lfqueue_enqueue(cobalt_lfqueue_t *queue, void *data);
int    cobalt_lfqueue_dequeue(cobalt_lfqueue_t *queue, void **out_data);
size_t cobalt_lfqueue_size(const cobalt_lfqueue_t *queue);
int    cobalt_lfqueue_is_empty(const cobalt_lfqueue_t *queue);
int    cobalt_lfqueue_is_full(const cobalt_lfqueue_t *queue);
size_t cobalt_lfqueue_capacity(const cobalt_lfqueue_t *queue);
```

- **`cobalt_lfqueue_enqueue`**: Enqueues item non-blockingly using Dmitry Vyukov's algorithm. Returns 0 on success, -1 if full or invalid arguments.
- **`cobalt_lfqueue_dequeue`**: Dequeues item non-blockingly. Stores dequeued pointer into `*out_data`. Returns 0 on success, -1 if empty.

---

## 3. Vector

**Header:** `include/cobalt/container/vector.h`

```c
cobalt_vector_t *cobalt_vector_create(size_t initial_capacity);
void             cobalt_vector_destroy(cobalt_vector_t *vec);
int              cobalt_vector_push(cobalt_vector_t *vec, void *item);
void            *cobalt_vector_get(cobalt_vector_t *vec, size_t index);
int              cobalt_vector_set(cobalt_vector_t *vec, size_t index, void *item);
size_t           cobalt_vector_size(cobalt_vector_t *vec);
int              cobalt_vector_is_empty(cobalt_vector_t *vec);
```

---

## 4. List & Deque

**Headers:** `include/cobalt/container/list.h`, `include/cobalt/container/deque.h`

```c
/* List */
cobalt_list_t *cobalt_list_create(void);
void           cobalt_list_destroy(cobalt_list_t *list);
int            cobalt_list_push_front(cobalt_list_t *list, void *item);
int            cobalt_list_push_back(cobalt_list_t *list, void *item);
void          *cobalt_list_pop_front(cobalt_list_t *list);
void          *cobalt_list_pop_back(cobalt_list_t *list);
void          *cobalt_list_get(cobalt_list_t *list, size_t index);
size_t         cobalt_list_size(cobalt_list_t *list);
int            cobalt_list_remove(cobalt_list_t *list, void *item);

/* Deque */
cobalt_deque_t *cobalt_deque_create(void);
void            cobalt_deque_destroy(cobalt_deque_t *deque);
int             cobalt_deque_push_front(cobalt_deque_t *deque, void *item);
int             cobalt_deque_push_back(cobalt_deque_t *deque, void *item);
void           *cobalt_deque_pop_front(cobalt_deque_t *deque);
void           *cobalt_deque_pop_back(cobalt_deque_t *deque);
void           *cobalt_deque_peek_front(cobalt_deque_t *deque);
void           *cobalt_deque_peek_back(cobalt_deque_t *deque);
size_t          cobalt_deque_size(cobalt_deque_t *deque);
```

---

## 5. Stack & Queue

**Headers:** `include/cobalt/container/stack.h`, `include/cobalt/container/queue.h`

```c
/* Stack */
cobalt_stack_t *cobalt_stack_create(void);
void            cobalt_stack_destroy(cobalt_stack_t *stack);
int             cobalt_stack_push(cobalt_stack_t *stack, void *item);
void           *cobalt_stack_pop(cobalt_stack_t *stack);
void           *cobalt_stack_peek(cobalt_stack_t *stack);
size_t          cobalt_stack_size(cobalt_stack_t *stack);

/* Queue */
cobalt_queue_t *cobalt_queue_create(void);
void            cobalt_queue_destroy(cobalt_queue_t *queue);
int             cobalt_queue_enqueue(cobalt_queue_t *queue, void *item);
void           *cobalt_queue_dequeue(cobalt_queue_t *queue);
void           *cobalt_queue_peek(cobalt_queue_t *queue);
size_t          cobalt_queue_size(cobalt_queue_t *queue);
```

---

## 6. Associative Containers (HashMap, TreeMap, Set)

**Headers:** `container/hashmap.h`, `container/treemap.h`, `container/set.h`

```c
/* HashMap */
cobalt_hashmap_t *cobalt_hashmap_create(size_t initial_buckets);
cobalt_hashmap_t *cobalt_hashmap_create_ext(size_t initial_buckets, cobalt_hash_func_t hash_func, cobalt_equal_func_t equal_func, cobalt_allocator_t *alloc);
void              cobalt_hashmap_destroy(cobalt_hashmap_t *map);
int               cobalt_hashmap_put(cobalt_hashmap_t *map, const char *key, void *value);
void             *cobalt_hashmap_get(cobalt_hashmap_t *map, const char *key);
int               cobalt_hashmap_remove(cobalt_hashmap_t *map, const char *key);
size_t            cobalt_hashmap_size(cobalt_hashmap_t *map);

/* TreeMap */
cobalt_treemap_t *cobalt_treemap_create(void);
cobalt_treemap_t *cobalt_treemap_create_ext(cobalt_compare_func_t compare_func);
void              cobalt_treemap_destroy(cobalt_treemap_t *map);
int               cobalt_treemap_put(cobalt_treemap_t *map, const char *key, void *value);
void             *cobalt_treemap_get(cobalt_treemap_t *map, const char *key);
int               cobalt_treemap_remove(cobalt_treemap_t *map, const char *key);
size_t            cobalt_treemap_size(cobalt_treemap_t *map);

/* Set */
cobalt_set_t *cobalt_set_create(size_t initial_capacity);
void          cobalt_set_destroy(cobalt_set_t *set);
int           cobalt_set_insert(cobalt_set_t *set, void *item);
int           cobalt_set_remove(cobalt_set_t *set, void *item);
int           cobalt_set_contains(cobalt_set_t *set, void *item);
size_t        cobalt_set_size(cobalt_set_t *set);
```

---

## 7. Polymorphic Interfaces & Iterators

### Sequence Interface (`Vector`, `List`, `Deque`)
Embeds `cobalt_sequence_t` as first member:
```c
cobalt_sequence_t *seq = (cobalt_sequence_t *)vec;
cobalt_iterator_t *iter = cobalt_iterator_new(seq);
while (cobalt_iterator_has_next(iter)) {
    void *item = cobalt_iterator_next(iter);
}
cobalt_iterator_destroy(iter);
```

### Map Interface (`HashMap`, `TreeMap`, `Set`)
Embeds `cobalt_map_t` as first member:
```c
cobalt_map_t *map = (cobalt_map_t *)hmap;
cobalt_map_iterator_t *iter = cobalt_map_iterator_create(map);
while (cobalt_map_iterator_has_next(iter)) {
    cobalt_map_pair_t pair = cobalt_map_iterator_next(iter);
    // pair.key, pair.value
}
cobalt_map_iterator_destroy(iter);
```

---

## 8. String Utilities

**Header:** `include/cobalt/utils/string.h`

```c
char **cobalt_split(const char *str, char delim, int *count);
char  *cobalt_join(const char **parts, char delim);
char  *cobalt_strip(const char *str);
```
