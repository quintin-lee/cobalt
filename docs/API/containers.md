# API Reference — Container Module (Layer 4)

**Module:** `include/cobalt/container/`
**Dependencies:** `interface/sequence.h`, `interface/map.h`, `interface/iterator.h`

## Overview

The container module provides 8 sequence and map containers. All containers store `void*` pointers — they do not own or free the pointed-to data. Users are responsible for managing the lifetime of the data.

| Container | Type | Time Complexity | Notes |
|-----------|------|----------------|-------|
| Vector | Dynamic array | O(1) push/get, O(n) insert/remove | 2x growth strategy |
| List | Doubly-linked | O(1) push/pop ends, O(n) random access | Bidirectional traversal |
| Stack | LIFO | O(1) push/pop/peek | Singly-linked list |
| Queue | FIFO | O(1) enqueue/dequeue/peek | Singly-linked list |
| Deque | Double-ended | O(1) push/pop both ends | Doubly-linked list |
| HashMap | Hash table | O(1) avg put/get/remove | Separate chaining, auto-resize at 75% |
| TreeMap | BST | O(log n) avg put/get/remove | String-keyed, ordered |
| Set | Hash set | O(1) avg insert/contains/remove | String-keyed only |

## Vector

```c
cobalt_vector_t *cobalt_vector_create(size_t initial_capacity);
void             cobalt_vector_destroy(cobalt_vector_t *vec);
int              cobalt_vector_push(cobalt_vector_t *vec, void *item);
void            *cobalt_vector_get(cobalt_vector_t *vec, size_t index);
int              cobalt_vector_set(cobalt_vector_t *vec, size_t index, void *item);
size_t           cobalt_vector_size(cobalt_vector_t *vec);
int              cobalt_vector_is_empty(cobalt_vector_t *vec);
```

**Behavior:**
- `cobalt_vector_create(0)` allocates a zero-capacity vector that grows on first push
- `cobalt_vector_push` doubles capacity when full (1 → 2 → 4 → ...)
- `cobalt_vector_get` returns NULL with `COBALT_ERROR_OUT_OF_BOUNDS` if index ≥ size
- `cobalt_vector_set` returns -1 with `COBALT_ERROR_INVALID_ARGUMENT` if index ≥ size
- Vector implements `cobalt_sequence_t` — use `(cobalt_sequence_t *)vec` for polymorphic ops
- **Memory note:** `destroy` frees internal array but NOT the data pointed to by elements

## List

```c
cobalt_list_t   *cobalt_list_create(void);
void             cobalt_list_destroy(cobalt_list_t *list);
int              cobalt_list_push_front(cobalt_list_t *list, void *item);
int              cobalt_list_push_back(cobalt_list_t *list, void *item);
void            *cobalt_list_pop_front(cobalt_list_t *list);
void            *cobalt_list_pop_back(cobalt_list_t *list);
void            *cobalt_list_get(cobalt_list_t *list, size_t index);
size_t           cobalt_list_size(cobalt_list_t *list);
int              cobalt_list_is_empty(cobalt_list_t *list);
cobalt_iterator_t *cobalt_list_iterator_create(cobalt_list_t *list);
int              cobalt_list_remove(cobalt_list_t *list, void *item);
```

**Behavior:**
- `cobalt_list_get` traverses from head or tail (whichever is closer) — at most n/2 hops
- `cobalt_list_remove` uses pointer equality (not value comparison)
- List implements `cobalt_sequence_t` — supports polymorphic `add`/`remove`
- `cobalt_list_pop_front`/`pop_back` return NULL on empty list (no error set)

## Stack

```c
cobalt_stack_t  *cobalt_stack_create(void);
void             cobalt_stack_destroy(cobalt_stack_t *stack);
int              cobalt_stack_push(cobalt_stack_t *stack, void *item);
void            *cobalt_stack_pop(cobalt_stack_t *stack);
void            *cobalt_stack_peek(cobalt_stack_t *stack);
size_t           cobalt_stack_size(cobalt_stack_t *stack);
int              cobalt_stack_is_empty(cobalt_stack_t *stack);
```

**Behavior:**
- LIFO — push and pop both operate on the top
- `cobalt_stack_pop` on empty returns NULL, sets no error
- Stack is singly-linked internally

## Queue

```c
cobalt_queue_t  *cobalt_queue_create(void);
void             cobalt_queue_destroy(cobalt_queue_t *queue);
int              cobalt_queue_enqueue(cobalt_queue_t *queue, void *item);
void            *cobalt_queue_dequeue(cobalt_queue_t *queue);
void            *cobalt_queue_peek(cobalt_queue_t *queue);
size_t           cobalt_queue_size(cobalt_queue_t *queue);
int              cobalt_queue_is_empty(cobalt_queue_t *queue);
```

**Behavior:**
- FIFO — enqueue at tail, dequeue from head
- `cobalt_queue_dequeue` on empty returns NULL, sets no error

## Deque

```c
cobalt_deque_t  *cobalt_deque_create(void);
void             cobalt_deque_destroy(cobalt_deque_t *deque);
int              cobalt_deque_push_front(cobalt_deque_t *deque, void *item);
int              cobalt_deque_push_back(cobalt_deque_t *deque, void *item);
void            *cobalt_deque_pop_front(cobalt_deque_t *deque);
void            *cobalt_deque_pop_back(cobalt_deque_t *deque);
void            *cobalt_deque_peek_front(cobalt_deque_t *deque);
void            *cobalt_deque_peek_back(cobalt_deque_t *deque);
size_t           cobalt_deque_size(cobalt_deque_t *deque);
int              cobalt_deque_is_empty(cobalt_deque_t *deque);
```

**Behavior:**
- Doubly-linked list implementation
- All operations are O(1)
- `pop_front`/`pop_back` on empty return NULL

## HashMap

```c
cobalt_hashmap_t *cobalt_hashmap_create(size_t initial_buckets);
void              cobalt_hashmap_destroy(cobalt_hashmap_t *map);
int               cobalt_hashmap_put(cobalt_hashmap_t *map, const char *key, void *value);
void             *cobalt_hashmap_get(cobalt_hashmap_t *map, const char *key);
int               cobalt_hashmap_remove(cobalt_hashmap_t *map, const char *key);
size_t            cobalt_hashmap_size(cobalt_hashmap_t *map);
size_t            cobalt_hashmap_capacity(const cobalt_hashmap_t *map);
```

**Behavior:**
- Keys are C-strings — an internal copy is made and stored
- `cobalt_hashmap_put` replaces existing value for same key (returns 0)
- Load factor threshold: 0.75 — triggers 2x rehash automatically
- `cobalt_hashmap_get` returns NULL (sets `COBALT_ERROR_NOT_FOUND`) if key not found
- `cobalt_hashmap_remove` returns -1 if key not found
- **String-keyed only** — not generic

## TreeMap

```c
cobalt_treemap_t *cobalt_treemap_create(void);
void              cobalt_treemap_destroy(cobalt_treemap_t *map);
int               cobalt_treemap_put(cobalt_treemap_t *map, const char *key, void *value);
void             *cobalt_treemap_get(cobalt_treemap_t *map, const char *key);
int               cobalt_treemap_remove(cobalt_treemap_t *map, const char *key);
const char        *cobalt_treemap_min_key(cobalt_treemap_t *map);
const char        *cobalt_treemap_max_key(cobalt_treemap_t *map);
size_t            cobalt_treemap_size(cobalt_treemap_t *map);
```

**Behavior:**
- Keys are C-strings compared lexicographically via `strcmp`
- `min_key`/`max_key` return NULL when tree is empty
- `cobalt_treemap_put` replaces existing value for same key
- True Red-Black tree — O(log n) worst case, O(log n) average
- **String-keyed only**

## Map Interface (HashMap, TreeMap, Set)

All three map containers embed  as their first member, enabling polymorphic use:



### Iterator API



### Concrete-type iterator factories



### Convenience API

All map operations have standalone convenience functions that accept :
, , , ,
, , .

---

## Map Interface (HashMap, TreeMap, Set)

All three map containers embed `cobalt_map_t` as their first member, enabling polymorphic use:

```c
cobalt_map_t *map = (cobalt_map_t *)cobalt_hashmap_create(8);
cobalt_map_put(map, "key", 4, value);
cobalt_map_get(map, "key", 4);
cobalt_map_remove(map, "key", 4);
cobalt_map_contains(map, "key", 4);
cobalt_map_clear(map);
cobalt_map_size(map);
cobalt_map_is_empty(map);
```

### Iterator API

```c
cobalt_map_iterator_t *iter = cobalt_map_iterator_create(map);
while (cobalt_map_iterator_has_next(iter)) {
    cobalt_map_pair_t pair = cobalt_map_iterator_next(iter);
    // pair.key   — pointer to key data
    // pair.value — pointer to value data
}
cobalt_map_iterator_destroy(iter);
```

### Concrete-type iterator factories

```c
cobalt_map_iterator_t *cobalt_hashmap_iterator_create(cobalt_hashmap_t *map);
cobalt_map_iterator_t *cobalt_treemap_iterator_create(cobalt_treemap_t *map);
cobalt_map_iterator_t *cobalt_set_iterator_create(cobalt_set_t *set);
```

### Convenience API

All map operations have standalone convenience functions that accept `cobalt_map_t *`:
`cobalt_map_get`, `cobalt_map_put`, `cobalt_map_remove`, `cobalt_map_contains`,
`cobalt_map_clear`, `cobalt_map_size`, `cobalt_map_is_empty`.

---

## Set

```c
cobalt_set_t *cobalt_set_create(size_t initial_capacity);
void          cobalt_set_destroy(cobalt_set_t *set);
int           cobalt_set_insert(cobalt_set_t *set, void *item);
int           cobalt_set_remove(cobalt_set_t *set, void *item);
int           cobalt_set_contains(cobalt_set_t *set, void *item);
size_t        cobalt_set_size(cobalt_set_t *set);
int           cobalt_set_is_empty(cobalt_set_t *set);
```

**Behavior:**
- Backed by HashMap — string-keyed only
- `cobalt_set_insert` is idempotent — inserting existing item returns 0
- `cobalt_set_contains` returns 1 if present, 0 otherwise
- **Warning:** string-based `cobalt_set_insert`/`cobalt_set_remove` cast items to `const char*`. For generic types, use the `_ext` variants.
- Implements `cobalt_map_t` — all map interface operations work polymorphically.

## Error Codes

All containers set errors via `cobalt_error_set()` on failure:

| Error | Triggered by |
|-------|-------------|
| `COBALT_ERROR_OUT_OF_MEMORY` | Allocation failure in create/push/put |
| `COBALT_ERROR_OUT_OF_BOUNDS` | `vector_get` with invalid index |
| `COBALT_ERROR_INVALID_ARGUMENT` | NULL pointer, out-of-range index |
| `COBALT_ERROR_NOT_FOUND` | `hashmap_get` key not found |

## Thread Safety

Containers themselves are **not thread-safe**. Concurrent access requires external synchronization. The `cobalt_sequence_t` interface methods (`add`, `remove`, `size`, `is_empty`, `iterator`) are atomic per-call but not safe for concurrent modification patterns.
