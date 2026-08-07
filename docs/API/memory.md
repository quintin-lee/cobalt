#API Reference — Memory Subsystem(Layer 7a)

**Module : ** `include / cobalt / memory /` **Dependencies
    : ** `platform
          .h`

      ##Overview

          The memory subsystem provides pluggable allocation strategies.All frameworks components
              use the default system allocator; custom allocators can be injected for arena-based or pool-based patterns.

## Allocator Interface

```c
typedef struct cobalt_allocator {
    void *(*alloc)(struct cobalt_allocator *self, size_t size);
    void (*free)(struct cobalt_allocator *self, void *ptr);
    void *(*realloc)(struct cobalt_allocator *self, void *ptr, size_t new_size);
} cobalt_allocator_t;
```

    The `self` pointer enables                        vtable -
    style                                             dispatch,
    allowing any allocation strategy to be swapped at runtime.

        ## #Functions

        | Function | Description | | -- -- -- -- --| -- -- -- -- -- -- -|
        | `cobalt_allocator_get_system()` | Get the default system allocator(wraps malloc / free) |
        | `cobalt_allocator_alloc(self, size)` | Allocate `size` bytes |
        | `cobalt_allocator_free(self, ptr)` | Free allocated block |
        | `cobalt_allocator_realloc(self, ptr, new_size)` | Resize allocation |

        All functions return `NULL` on failure.Callers must check return values.

            ##Arena Allocator

```c
#include "cobalt/memory/arena.h"

            cobalt_arena_t
            * cobalt_arena_create(size_t initial_size);
void  cobalt_arena_destroy(cobalt_arena_t *arena);
void *cobalt_arena_alloc(cobalt_arena_t *arena, size_t size);
void  cobalt_arena_reset(cobalt_arena_t *arena);
```

    ## #Behavior

    - **`create(size)`** : Allocates a contiguous buffer of `size` bytes -
    **`alloc(arena, size)`** : Bump - pointer allocation; returns aligned pointer
- **`reset(arena)`**: Sets used offset to 0 — O(1) "free" of all allocations
- **`destroy(arena)`**: Frees the underlying buffer

### Use Cases

- Per-frame or per-request transient allocations
- Batch object creation where all objects share a lifetime
- Reducing fragmentation in long-running applications

### Lifetime Warning

After `cobalt_arena_reset()` or `cobalt_arena_destroy()`, **all pointers obtained from that arena become invalid**. Do not access freed arena memory.

## Future: Pool & Slab Allocators

`PoolAllocator` (fixed-size block pool) and `SlabAllocator` (object-caching slab) are planned but not yet implemented. See `docs/SPEC/memory.md` for design details.
