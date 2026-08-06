# Design: Map Abstract Interface

**Date:** 2026-08-06
**Status:** Approved
**Scope:** Layer 5 Interface (`include/cobalt/interface/map.h`)

## 1. Problem

HashMap and TreeMap share the same key-value operations (put/get/remove/size) but have no common abstract type. Algorithms that work with maps must duplicate logic for each concrete type. The SPEC at `docs/SPEC/interfaces.md` documented a Map interface that was never implemented.

## 2. Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Key type in interface | `const void *key, size_t key_len` | Generic — works for strings, ints, structs |
| Iterator design | Dedicated `cobalt_map_iterator_t` | Yields `cobalt_map_pair_t {key, value}` — semantically correct for maps |
| Vtable style | Embedded function pointers | Matches existing `cobalt_sequence_t` pattern exactly |
| Destroy in interface | No | Concrete types already have their own destroy; adding it would require users to track map type |
| Key lifetime | Caller-managed (no copy) | Consistent with `_ext` API already in HashMap; string-based API handles its own copying |

## 3. Interface Definition

### 3.1 Map struct

```c
typedef struct cobalt_map cobalt_map_t;

typedef struct cobalt_map_pair {
    const void *key;
    void       *value;
} cobalt_map_pair_t;

struct cobalt_map {
    /* Get value for key. Returns NULL if not found. */
    void *(*get)(cobalt_map_t *self, const void *key, size_t key_len);

    /* Insert or update key-value pair. Returns 0 on success, -1 on failure. */
    int  (*put)(cobalt_map_t *self, const void *key, size_t key_len, void *value);

    /* Remove key-value pair. Returns 0 on success, -1 if not found. */
    int  (*remove)(cobalt_map_t *self, const void *key, size_t key_len);

    /* Number of entries. */
    size_t (*size)(cobalt_map_t *self);

    /* Non-zero if empty. */
    int    (*is_empty)(cobalt_map_t *self);

    /* Create iterator for this map. Caller must destroy with cobalt_map_iterator_destroy(). */
    cobalt_iterator_t *(*iterator)(cobalt_map_t *self);
};
```

### 3.2 Map Iterator

```c
typedef struct cobalt_map_iterator cobalt_map_iterator_t;

typedef struct cobalt_map_iterator_vtable {
    int  (*has_next)(void *ctx);
    cobalt_map_pair_t (*next)(void *ctx);  /* Returns {NULL, NULL} when exhausted */
    void (*destroy)(void *ctx);
} cobalt_map_iterator_vtable_t;

struct cobalt_map_iterator {
    const cobalt_map_iterator_vtable_t *vtable;
    void *data;
};

/* Public API */
cobalt_map_iterator_t *cobalt_map_iterator_create(cobalt_map_t *map);
int  cobalt_map_iterator_has_next(cobalt_map_iterator_t *iter);
cobalt_map_pair_t cobalt_map_iterator_next(cobalt_map_iterator_t *iter);
void cobalt_map_iterator_destroy(cobalt_map_iterator_t *iter);
```

### 3.3 Key Type

The Map interface uses `const void *key + size_t key_len` for all operations. This enables polymorphic use with any key type. The string-specific API on concrete types (e.g., `cobalt_hashmap_put(map, "key", value)`) is preserved as a convenience wrapper.

## 4. Concrete Type Changes

### 4.1 HashMap

- Add `cobalt_map_t map;` as first member of `struct cobalt_hashmap`
- Initialize `map` fields in `cobalt_hashmap_create()` and `cobalt_hashmap_create_ext()`
- Implement `cobalt_hashmap_iterator_create()` returning a map iterator
- Existing string API unchanged

### 4.2 TreeMap

- Add `cobalt_map_t map;` as first member of `struct cobalt_treemap`
- Initialize `map` fields in `cobalt_treemap_create()`
- Implement `cobalt_treemap_iterator_create()` returning a map iterator
- Existing string API unchanged (including min_key/max_key which are TreeMap-specific)

## 5. Usage Example

```c
/* Polymorphic map operations */
cobalt_map_t *map = (cobalt_map_t *)cobalt_hashmap_create(16);

int key = 42;
int val = 100;
cobalt_map_put(map, &key, sizeof(int), &val);

void *result = cobalt_map_get(map, &key, sizeof(int));

/* Iterate */
cobalt_map_iterator_t *iter = cobalt_map_iterator_create(map);
while (cobalt_map_iterator_has_next(iter)) {
    cobalt_map_pair_t pair = cobalt_map_iterator_next(iter);
    printf("key=%p value=%p\n", pair.key, pair.value);
}
cobalt_map_iterator_destroy(iter);

/* Cleanup via concrete type */
cobalt_hashmap_destroy((cobalt_hashmap_t *)map);
```

## 6. Testing

- `tests/unit/test_map.c`: polymorphic tests using `cobalt_map_t *` with both HashMap and TreeMap
- Verify get/put/remove/size/is_empty/iterator through interface
- Verify key-value pairs are correctly yielded
- Verify polymorphic cast safety (HashMap* → cobalt_map_t* → TreeMap*)

## 7. Files Changed

| File | Change |
|------|--------|
| `include/cobalt/interface/map.h` | **New** — Map interface and iterator definitions |

| `include/cobalt/container/hashmap.h` | Add iterator factory declaration |
| `include/cobalt/container/treemap.h` | Add iterator factory declaration |
| `src/container/hashmap.c` | Embed `cobalt_map_t base`, implement iterator |
| `src/container/treemap.c` | Embed `cobalt_map_t base`, implement iterator |
| `src/interface/map.c` | **New** — iterator implementation |
| `include/cobalt/cobalt.h` | Include map.h |
| `tests/unit/test_map.c` | **New** — polymorphic map tests |
| `tests/CMakeLists.txt` | Register test_map |
