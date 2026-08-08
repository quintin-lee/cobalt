# SPEC: Concrete Collections (Layer 4)

**Module:** `include/cobalt/container/`, `src/container/`  
**Files:** vector.h, list.h, hashmap.h, treemap.h, and corresponding .c files

## 1. Overview

Concrete collection types implement the Layer 5 interface contracts. Each is optimized for different access patterns and use cases.

## 2. Vector (Dynamic Array)

**Header:** `container/vector.h`  
**Source:** `src/container/vector.c`  
**Interfaces:** `Sequence` + `Iterable`

Properties:
- Contiguous memory storage (cache-friendly)
- O(1) random access by index
- Amortized O(1) append (push_back)
- O(n) insertion/removal in middle
- Grows capacity by doubling when full

Typical use: when you need fast indexed access and mostly append-only growth.

## 3. List (Doubly-Linked List)

**Header:** `container/list.h`  
**Source:** `src/container/list.c`  
**Interfaces:** `Sequence` + `Iterable`

Properties:
- Non-contiguous node allocation
- O(1) front/back push/pop
- O(n) random access (linear scan)
- Constant-time insert/remove given iterator/node position
- Higher memory overhead per element (prev+next pointers)

Typical use: frequent insert/delete at head/tail, or when you need stable node references.

## 4. HashMap (Hash Table)

**Header:** `container/hashmap.h`  
**Source:** `src/container/hashmap.c`  
**Interfaces:** `Map` + `Iterable`

Properties:
- Key-based lookup (string keys, hashed to bucket)
- O(1) average-case get/put/remove
- No guaranteed order iteration
- Collision chaining handles hash conflicts
- Load factor threshold triggers rehashing

Typical use: fast key-value lookup where ordering doesn't matter.

## 5. TreeMap (Red-Black Tree)

**Header:** `container/treemap.h`  
**Source:** `src/container/treemap.c`  
**Interfaces:** `Map` + `Iterable`

Properties:
- Self-balancing binary search tree (red-black)
- O(log n) guaranteed get/put/remove
- Keys maintained in sorted order
- Iterator traverses in-key-order

Typical use: when you need ordered key traversal with guaranteed logarithmic performance.

## 6. Allocator Integration

All collections accept allocator injection at construction. For example:

```c
// Create a vector using arena allocator
cobalt_allocator_t *arena_alloc = get_arena_allocator();
cobalt_vector_t *vec = vector_create_with_allocator(arena_alloc, 16);
```

All internal allocations go through the provided allocator, enabling whole-heap-free operation when combined with an arena/pool allocator.

## 6. HashMap Runtime API

**Header:** `container/hashmap.h`  
**Source:** `src/container/hashmap.c`

These APIs allow runtime modification of hashmap behavior and allocator-compatible creation.

```c
// Create with explicit initial bucket count (prevents first-rehash overhead)
cobalt_hashmap_t *cobalt_hashmap_create_ext(size_t initial_buckets, cobalt_allocator_t *alloc);

// Replace hash and equality callbacks on an existing hashmap
// Changes only affect future operations - existing entries are not rehashed
int cobalt_hashmap_set_funcs(cobalt_hashmap_t *map,
                              cobalt_hash_func_t  hash_func,
                              cobalt_equal_func_t equal_func);
```

**`cobalt_hashmap_create_ext`**: Creates a HashMap with a user-specified initial bucket count. Use this when you know the expected size range to avoid rehashing overhead.

**`cobalt_hashmap_set_funcs`**: Replaces the hash and equality callbacks on an existing hashmap. Changes only affect future operations — existing entries are not rehashed. Useful for swapping between string and integer key modes, or for testing with mock functions.

## 7. TreeMap Custom Comparator

**Header:** `container/treemap.h`  
**Source:** `src/container/treemap.c`

```c
// Create a TreeMap with a custom comparison function
cobalt_treemap_t *cobalt_treemap_create_ext(cobalt_compare_func_t compare_func);
```

**`cobalt_treemap_create_ext`**: Creates a TreeMap with a custom comparison function. Use `NULL` for default `strcmp` behavior (same as `cobalt_treemap_create()`). The comparator receives `(const void *a, const void *b)` and must return `< 0`, `0`, or `> 0`.

Typical use: ordering non-string keys (integers, structs, pointers) or custom string comparison (case-insensitive, locale-aware).

## 8. String Utilities

**Header:** `utils/string.h`  
**Source:** `src/utils/string.c`

```c
// Split a string by delimiter, returns array of strings (caller must free each element and the array)
char **cobalt_split(const char *str, char delim, int *count);

// Join an array of strings with a delimiter
char *cobalt_join(const char **parts, char delim);

// Strip leading/trailing whitespace
char *cobalt_strip(const char *str);
```

| Function | Description |
|----------|-------------|
| `cobalt_split` | Split str by delim, store result in **parts (caller must free each element and the array). count receives the number of parts. |
| `cobalt_join` | Join an array of strings with the given delimiter. Returns a newly allocated string (caller must free). |
| `cobalt_strip` | Strip leading and trailing whitespace from str. Returns a newly allocated string (caller must free). |

**Example:**
```c
int count;
char **parts = cobalt_split("a,b,c", ',', &count);
// parts[0]="a", parts[1]="b", parts[2]="c", count=3
for (int i = 0; i < count; i++) free(parts[i]);
free(parts);

char *joined = cobalt_join(parts, ',');  // "a,b,c"
free(joined);

char *trimmed = cobalt_strip("  hello  ");  // "hello"
free(trimmed);
```

> **Security note**: `cobalt_split` had a heap-buffer-overflow fix in v2.3.0 (CWE-122). The allocation size was corrected from `sizeof(char*)*cnt + 1` to `sizeof(char*)*(cnt+1)` to account for the NULL terminator.

## 7. Future Collections (Planned)

- Stack (LIFO queue) — based on vector or list
- Queue (FIFO) — based on circular buffer or doubly-linked list
- Set (unique-key collection) — could wrap HashMap or use separate data structure
