# SPEC: Memory Subsystem (Layer 7a)

**Module:** `include/cobalt/memory/`, `src/memory/`  
**Files:** allocator.h, arena.h, allocator.c, arena.c

## 1. Overview

The Memory subsystem provides flexible allocation strategies that can be injected into any component, enabling memory autonomy across the entire framework. All allocations go through an allocator vtable, allowing runtime swap of allocation strategies without changing client code.

## 2. Allocator Interface

```c
typedef struct cobalt_allocator {
    void *(*alloc)(struct cobalt_allocator *self, size_t size);
    void (*free)(struct cobalt_allocator *self, void *ptr);
    void *(*realloc)(struct cobalt_allocator *self, void *ptr, size_t new_size);
} cobalt_allocator_t;
```

The `self` pointer enables a vtable-style interface where any allocator implementation provides its own function pointers.

## 3. Allocator Implementations

### 3.1 SystemAllocator

Wraps standard `malloc`/`free`/`realloc`:

```c
static const cobalt_allocator_t system_allocator = {
    .alloc  = malloc,
    .free   = free,
    .realloc = realloc
};
```

Default allocator used when no custom strategy is specified.

### 3.2 ArenaAllocator (Region-based)

Pre-allocates a contiguous memory block. Allocation proceeds by bumping a pointer; destruction resets the entire arena (O(1) cleanup). Ideal for transient workloads or per-frame allocation patterns.

**Key functions:**
- `cobalt_arena_create(size)` — allocate arena buffer
- `cobalt_arena_alloc(arena, size)` — bump-pointer allocation
- `cobalt_arena_reset(arena)` — deallocate all at once
- `cobalt_arena_destroy(arena)` — free buffer

### 3.3 PoolAllocator (Planned)

Future extension: fixed-size block pool to prevent fragmentation for frequent small allocations.

### 3.4 SlabAllocator (Planned)

Future extension: object-caching slab allocator for predictable allocation latency.

## 4. Allocator Injection Pattern

Objects accept an allocator parameter at construction, stored externally to keep object headers compact:

```c
typedef struct {
    cobalt_object_t base;     // Standard object header (ref_count + class)
    MyData data;              // Application-specific payload
    cobalt_allocator_t *alloc; // Allocator for data memory
} MyDerivedObject;

MyDerivedObject *myobj = cobalt_object_new_with_allocator(
    &MyDerivedClass, sizeof(MyData), &arena_allocator);
```

This pattern keeps the object header fixed-size while allowing per-object memory strategy customization.

## 5. Error Handling on Allocation

All allocator functions return `NULL` on failure. Higher-level constructors should check and propagate errors via the Runtime Error subsystem (Layer 7b).
