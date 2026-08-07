# Migration Guide: Cobalt v1 → v2

This guide covers breaking changes between Cobalt v1.x and v2.x.

## 1. Allocator Injection Pattern

**v1:** Allocators were embedded directly in struct members:
```c
// v1 style
typedef struct {
    cobalt_allocator_t alloc;  // embedded
    ...
} cobalt_vector_t;
```

**v2:** Allocators are passed via constructor and stored externally:
```c
// v2 style
cobalt_vector_t *v = cobalt_vector_create_with_allocator(capacity, &my_alloc);
```

### What changed
- All container constructors now accept an optional `cobalt_allocator_t *` parameter
- The allocator is stored in a separate allocation (not in the struct header)
- This keeps object headers compact and enables true allocator injection

### Migration steps
1. Replace `cobalt_vector_create(n)` → `cobalt_vector_create_with_allocator(n, NULL)` for default behavior
2. For custom allocators, pass your allocator: `cobalt_vector_create_with_allocator(n, &my_alloc)`
3. Remove any direct `alloc` member access from container structs

## 2. Error Handling API

**v1:** Errors were returned via global thread-local variable:
```c
// v1
cobalt_error_t err = cobalt_last_error();
```

**v2:** Errors can be captured via pointer or accessed via getter:
```c
// v2
cobalt_error_t err = COBALT_SUCCESS;
cobalt_error_set(&err, COBALT_ERROR_OUT_OF_MEMORY);
// or
err = cobalt_error_get_current();
```

### What changed
- Added `cobalt_error_stack` for saving/restoring error state in nested calls
- `cobalt_error_set()` now accepts a pointer for conditional error capture
- Error codes now use `COBALT_ERROR_*` prefix consistently

## 3. Container Constructor Signatures

| Container | v1 Constructor | v2 Constructor |
|-----------|---------------|----------------|
| Vector | `cobalt_vector_create(capacity)` | `cobalt_vector_create_with_allocator(capacity, alloc)` |
| HashMap | `cobalt_hashmap_create(buckets)` | `cobalt_hashmap_create_with_allocator(buckets, alloc)` |
| List | `cobalt_list_create()` | `cobalt_list_create_with_allocator(alloc)` |
| Stack | `cobalt_stack_create()` | `cobalt_stack_create_with_allocator(alloc)` |
| Queue | `cobalt_queue_create()` | `cobalt_queue_create_with_allocator(alloc)` |
| Set | `cobalt_set_create(capacity)` | `cobalt_set_create_with_allocator(capacity, alloc)` |
| Deque | `cobalt_deque_create()` | `cobalt_deque_create_with_allocator(alloc)` |
| TreeMap | `cobalt_treemap_create()` | *(no allocator variant yet)* |

**Default behavior:** Pass `NULL` for the allocator to use the system allocator (same as v1).

## 4. New Modules in v2

- **Stream operators** (`cobalt/algorithm/stream.h`): `take`, `drop`, `take_while`, `drop_while`
- **Error stack** (`cobalt/runtime/error_stack.h`): Save/restore error state
- **Pool/Slab allocators** (`cobalt/memory/pool.h`, `cobalt/memory/slab.h`): Fixed-size block pools
- **Thread primitives**: Mutex, condition variable, thread create/join

## 5. Removed/Deprecated

- No public APIs were removed in v2
- All v1 code continues to work with `NULL` allocator parameter
- The `my_strdup` utility was renamed to `cobalt_strdup()`

## Quick Migration Checklist

- [ ] Update all container creation calls to include allocator parameter (pass `NULL` for default)
- [ ] Update error handling to use `cobalt_error_set(&err, code)` pattern
- [ ] Test with custom allocator to verify injection works
- [ ] Run valgrind/ASan to check for memory issues
- [ ] Update CI to use new CMake options (`COBALT_RUN_TIDY`, `COBALT_RUN_FORMAT`)
