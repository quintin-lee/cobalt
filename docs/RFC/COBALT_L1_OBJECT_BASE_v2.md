# RFC: Cobalt (libcobalt) v2.0.0 Architecture Baseline

**Title:** Cobalt Object Base & Lightweight Technology — C11 Foundation for Embedded and High-Performance Systems  
**Author:** Cobalt Development Team  
**Status:** Proposal (v2.0.0 Baseline)  
**Date:** 2026-07-29  
**Version:** 2.0.0  
**Related Files:** `include/cobalt/cobalt.h`, `src/core/*.c`, `cmake/CMakeLists.txt`

---

## 1. Executive Summary

Cobalt is a lightweight, zero-runtime-dependence C11 infrastructure library that brings object-oriented capabilities to C while maintaining strict control over memory layout and performance. It provides single-inheritance with multi-interface support, read-only vtables, RTTI type checking, and atomically-referenced object lifecycles — all injectable via custom allocators (Arena/Pool/Slab). The framework is designed for deep embedding in resource-constrained environments (embedded systems, HFT components, high-performance servers) with minimal code footprint (< 50 KB compiled size on typical 64-bit targets).

---

## 2. Design Goals & Core Philosophy

### 2.1 Lightweight (Lightweight)
- No third-party mandatory dependencies — only standard C11 library (`<stddef.h>`, `<stdint.h>`, `<stdatomic.h>`, `<limits.h>`)
- Minimal compiled code footprint; supports `-Os` (size-optimized) compilation
- Supports cross-compilation for bare-metal and constrained environments

### 2.2 Object Base (Object Base)
- Safe single-inheritance + multi-interface mechanism in pure C11
- Read-only vtables (stored in `.rodata` section) for safety and sharing across processes
- RTTI-enabled type introspection and safe interface casting
- Atomic reference-counted lifecycle management (no GC, no hidden allocations)

### 2.3 Memory Autonomy (Memory Autonomy)
- Every component and object lifecycle transparently accepts a custom allocator injection
- Rejects hard-coded global `malloc`/`free` by default; system allocator provided but optional
- Supports arena-based, slab, and pool allocation strategies without API change

### 2.4 Strict Layering (Strict Layering)
- Eight-layer architecture with **one-way dependency**: upper layers may only depend on lower layers
- No cyclic or backward references enforced at compile time via naming and include path discipline
- Enables modular replacement of any layer independently (e.g., swap platform layer for different OS backends)

---

## 3. Eight-Layer Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│          Layer 1: Applications & Extensions                │
│            (Business logic / Domain extensions)            │
└──────────────▲─────────────────────────────────────────────┘
               │
┌──────────────▼─────────────────────────────────────────────┐
│          Layer 2: Modules & Utilities                      │
│           (JSON/XML serialization / Event loop / Signals)  │
└──────────────▲─────────────────────────────────────────────┘
               │
┌──────────────▼─────────────────────────────────────────────┐
│          Layer 3: Algorithms & Functional Streams         │
│         (Generic sort / binary search / map/filter/fold)  │
└──────────────▲─────────────────────────────────────────────┘
               │
┌──────────────▼─────────────────────────────────────────────┐
│          Layer 4: Concrete Collections & Adapters         │
│           (Vector / HashMap / TreeMap / Stack / Queue)    │
└──────────────▲─────────────────────────────────────────────┘
               │
┌──────────────▼─────────────────────────────────────────────┐
│          Layer 5: Container Interfaces                    │
│        (Sequence / Map / Set / Iterable / Iterator)       │
└──────────────▲─────────────────────────────────────────────┘
               │
┌──────────────▼─────────────────────────────────────────────┐
│          Layer 6: Core Object System & RTTI              │
│         (CobaltObject / CobaltClass / Interface Table)   │
└──────────────┬─────────────────────────────────────────────┘
               │   ┌──────────────────────────────────────┐
               ▼   │                                    │
┌──────────────┴───┴────────────────────────────────────┐
│          Layer 7a: Memory Subsystem                    │
│         (CobaltAllocator / Pool / Slab / Arena)       │
│                                                       │
│          Layer 7b: Runtime Subsystem                   │
│         (Error Stack / Logger / Thread-local state)   │
└──────────────┬──────────────────────────────────────────┘
               │
┌──────────────▼─────────────────────────────────────────────┐
│          Layer 8: Platform & Kernel Abstraction (OAL)      │
│              (OS translation / Atomics / Alignment / IO)  │
└───────────────────────────────────────────────────────────┘
```

---

## 4. Layer-by-Layer Specification

### 4.1 Layer 8: Platform & Kernel Abstraction (OAL)

**Module Path:** `include/cobalt/platform/` / `src/platform/`  
**Core Files:** `platform.h`, `atomic.h`, `platform.c`, `atomic.c`

Provides hardware/OS-independent primitives required by all upper layers:

| Component | Header | Purpose |
|-----------|--------|---------|
| Platform detection | `platform.h` | Compile-time OS/target identification (`_WIN32`, `__APPLE__`, `__linux__`) |
| Atomic operations | `atomic.h` | Thread-safe integer operations wrapped around `<stdatomic.h>` |
| Alignment utilities | *(to be added)* | Aligned memory allocation helpers |
| Byte order conversion | *(to be added)* | Endian conversion macros/functions |

**Interface guarantees:** All functions are free from dynamic allocation; all atomics lock-free where possible on target architecture.

### 4.2 Layer 7: Memory Subsystem

**Module Path:** `include/cobalt/memory/` / `src/memory/`  
**Core Files:** `allocator.h`, `arena.h`, `allocator.c`, `arena.c`

Injectable allocation abstractions enabling per-object memory autonomy:

```c
// Allocator vtable (read-only, placed in .rodata)
typedef struct cobalt_allocator {
    void *(*alloc)(struct cobalt_allocator *self, size_t size);
    void (*free)(struct cobalt_allocator *self, void *ptr);
    void *(*realloc)(struct cobalt_allocator *self, void *ptr, size_t new_size);
} cobalt_allocator_t;
```

**Allocator types:**
- `SystemAllocator`: wraps `malloc`/`free`/`realloc` — used when no custom allocator is provided
- `ArenaAllocator`: region-based allocation; all objects freed at once via `arena_reset()` (ideal for transient workloads)
- `PoolAllocator`: fixed-size block pool (prevents fragmentation, O(1) allocation/free)
- `SlabAllocator`: object-caching slab allocator (for frequent small-object creation)

All objects in Cobalt accept an allocator pointer at construction, stored separately from the object's vtable.

### 4.3 Layer 7: Runtime Subsystem

**Module Path:** `include/cobalt/runtime/` / `src/runtime/`  
**Core Files:** `error.h`, `logger.h`, `error.c`, `logger.c`

Runtime services independent of core object semantics:

| Component | Header | Purpose |
|-----------|--------|---------|
| Error handling | `error.h` | Stackable error codes with descriptive messages; thread-local error state |
| Logging | `logger.h` | Structured logging facility with configurable severity levels (TRACE → FATAL) |

**Error codes:** Enumerated values (`COBALT_SUCCESS`, `COBALT_ERROR_INVALID_ARGUMENT`, `COBALT_ERROR_OUT_OF_MEMORY`, etc.) mapped to human-readable strings via `cobalt_error_get_message()`.

### 4.4 Layer 6: Core Object System

**Module Path:** `include/cobalt/core/` / `src/core/`  
**Core Files:** `object.h`, `class.h`, `interface.h`, `object.c`, `class.c`, `interface.c`

The heart of Cobalt's OO capabilities:

#### 4.4.1 CobaltObject (Base Class)

```c
typedef struct cobalt_object {
    uint64_t ref_count;           // Atomic ref-count (RCU-safe)
    cobalt_class_t *class;        // Pointer to class metadata (vtable)
} cobalt_object_t;
```

Every publicly-exposed object begins with this header, allowing polymorphic treatment as `cobalt_object_t*`. Reference counting uses atomic operations for thread-safe lifetime management.

#### 4.4.2 CobaltClass (Type Metadata)

```c
typedef struct cobalt_method {
    const char *name;
    void *(*invoke)(cobalt_object_t *self, void **args, size_t arg_count);
} cobalt_method_t;

typedef struct cobalt_property {
    const char *name;
    void *(*get)(cobalt_object_t *self);
    void (*set)(cobalt_object_t *self, void *value);
} cobalt_property_t;

typedef struct cobalt_class {
    const char *name;                           // Class name (for RTTI)
    size_t method_count;
    cobalt_method_t **methods;                  // Method table (vtable)
    size_t property_count;
    cobalt_property_t **properties;
    cobalt_class_t *base_class;                 // Single inheritance chain
    int abstract;                               // Whether class is abstract
} cobalt_class_t;
```

Vtables are constructed at initialization (or statically for leaf classes) and stored in read-only memory after registration to prevent runtime tampering.

#### 4.4.3 Interface Mechanism (Multi-injection)

Interfaces are pure abstract base classes with no data members, only virtual function pointers. An object implements multiple interfaces by providing interface-specific vtables accessible via `cobalt_object_query_interface()` (similar to COM/IUnknown style):

```c
typedef struct cobalt_interface_vtable {
    void (*destroy)(cobalt_interface_t *self);
    // Interface-specific virtual methods...
} cobalt_interface_vtable_t;

typedef struct cobalt_interface {
    cobalt_interface_vtable_t *vtable;
} cobalt_interface_t;

int cobalt_object_implements(cobalt_object_t *obj, cobalt_interface_t *iface);
```

This enables true multiple inheritance without diamond problems: an object has one `base_class` (single inheritance) plus arbitrary number of implemented interfaces.

### 4.5 Layer 5: Container Interfaces

**Module Path:** `include/cobalt/interface/`  
**Core Files:** `sequence.h`, `map.h`, `iterator.h`

Abstract base interfaces defining the contract for all collections:

| Interface | Header | Key Operations |
|-----------|--------|----------------|
| Sequence | `sequence.h` | `size()`, `is_empty()`, `add()`, `remove()`, `iterator()` |
| Map | `map.h` | `get(key)`, `put(key,value)`, `remove(key)`, `size()`, `is_empty()` |
| Iterable | *(to be added)* | `iterator()` returning a traversal cursor |
| Iterator | `iterator.h` | `has_next()`, `next()`, `destroy()` |

Each interface defines a set of function pointers forming its vtable. Concrete containers implement these interfaces by embedding the interface struct as their first member (ensuring ABI compatibility).

### 4.6 Layer 4: Concrete Collections

**Module Path:** `include/cobalt/container/` / `src/container/`  
**Core Files:** `vector.h`, `list.h`, `hashmap.h`, `treemap.h`, `vector.c`, `list.c`, `hashmap.c`, `treemap.c`

Implementations of the Layer 5 interfaces:

| Collection | Header | Underlying Structure | Characteristics |
|------------|--------|---------------------|-----------------|
| Vector | `vector.h` | Dynamic array | O(1) random access, amortized O(1) push, O(n) insert/remove |
| List | `list.h` | Doubly-linked list | O(1) front/back push/pop, O(n) random access |
| HashMap | `hashmap.h` | Chained hash table | O(1) average lookup/insert, unordered iteration |
| TreeMap | `treemap.h` | Red-black tree | O(log n) lookup/insert, sorted traversal by key |

All containers support allocator injection and implement the appropriate interface(s) (e.g., `Vector` implements both `Sequence` and `Iterable`; `HashMap` implements `Map`).

### 4.7 Layer 3: Algorithms & Functional Streams

**Module Path:** `include/cobalt/algorithm/` / `src/algorithm/`  
**Core Files:** `sort.h`, `functional.h`, `sort.c`, `functional.c`

Generic algorithms operating on `Sequence`/`Iterable` protocols:

| Algorithm | Header | Description |
|-----------|--------|-------------|
| Sorting | `sort.h` | Quicksort, insertion sort, merge-sort variants |
| Predicates | `functional.h` | Equality, null checks, composition combinators |
| Stream ops | *(future)* | `map()`, `filter()`, `fold()` lazy streams over iterables |

These operate purely through the interface contracts (Layer 5), decoupling algorithm implementation from container storage.

### 4.8 Layer 2: Modules & Utilities

**Module Path:** `include/cobalt/module/` / `src/module/`  
**Core Files:** `json.h`, `eventloop.h`, `json.c`, `eventloop.c`

Domain-level building blocks atop the foundation layers:

| Module | Header | Description |
|--------|--------|-------------|
| JSON serialization/deserialization | `json.h` | Parser from C string, serializer to C string, tree-based in-memory representation |
| Event loop | `eventloop.h` | FD/watch timer notification loop ( epoll/kqueue / IOCP abstraction ), callback-driven |
| Signal/Slot | *(future)* | Lightweight publish-subscribe with type-safe payloads |

These modules use Layer 4 collections internally and may employ Layer 3 algorithms for processing.

### 4.9 Layer 1: Applications & Extensions

**Path:** `cobalt/app/` *(user-provided)*

Application-specific code that leverages Cobalt libraries. This layer is outside the framework itself but follows Cobalt's conventions: applications create concrete objects, pass them through interfaces, and consume utility modules (JSON, event loop) as needed.

---

## 5. Core Component Interaction Flow

Example: Create a HashMap using an Arena allocator, then iterate with COLLET_FOREACH macro.

| Step | Layer | Component | Action |
|------|-------|-----------|--------|
| 1 | L1 | Application | Calls `cobalt_map_new_with_arena(arena, initial_buckets)` |
| 2 | L6 | `CobaltClass::create` | Allocates memory from arena (via allocator vtable), initializes object header with `ref_count=1`, sets `class` pointer to HashMap's class |
| 3 | L7 | Arena Allocator | Provides contiguous pre-allocated memory block; no individual heap calls |
| 4 | L4 | `CobaltHashMap` | Initializes bucket array from arena memory, embeds `CobaltMap` interface vtable |
| 5 | L5 | `CobaltMap` interface | User code calls `cobalt_map_put(map, key, value)` → dispatches to HashMap's put method via interface vtable |
| 6 | L3 | `COLL_FOREACH` macro | Obtains iterator via `map->iterator(interface)`, loops with `iterator_has_next()/iterator_next()` |
| 7 | L8 | Atomic ops | Reference count increments/decrements use `stdatomic` operations for thread safety |
| 8 | L1 | Application | Uses iterated data, eventually unrefs map; arena reset frees all at once |

This demonstrates the clean separation of concerns: the application never touches arena internals, the hashmap never knows about the iteration protocol, and everything relies solely on interface contracts above it.

---

## 6. Build & Integration

### 6.1 Build System (CMake)

Project uses CMake 3.14+ with the following features:

- `enable_testing()` with CTest integration
- Automatic generation of `cobalt.pc` pkg-config file via `configure_file()`
- Export of `cobaltConfig.cmake` for `find_package(Cobalt)` consumption
- Build-type dependent optimization flags (`-O2 -DNDEBUG` for Release, `-O0 -g` for Debug)

### 6.2 Compiler Configuration

`.clangd` configuration includes:
- Points to generated `compile_commands.json` for IDE support
- Includes `-Iinclude` and `-Iinclude/cobalt` as standard search paths
- Warning flags: `-Wall -Wextra -Wpedantic -Werror=implicit-function-declaration`

### 6.3 Header Guard Convention

Every public header uses unique include guards named after the module path in uppercase, e.g.:

```c
#ifndef COBALT_OBJECT_H
#define COBALT_OBJECT_H
/* ... */
#endif /* COBALT_OBJECT_H */
```

---

## 7. Compatibility & ABI Stability

- **Source compatibility**: Cobalt aims for stable source API across minor revisions (v2.x)
- **Binary stability**: All vtables are const-qualified and stored in read-only sections; applications linking against a specific major version should not expect ABI changes between patch releases
- **Zero runtime**: No hidden constructors/destructors; all object lifecycle explicit via `new`/`destroy` or arena-reset patterns

---

## 8. Open Questions & Future Work

| Item | Status | Notes |
|------|--------|-------|
| `Stack` / `Queue` concrete containers | Planned | Extend Layer 4 collection suite |
| Lazy functional stream operators (`map`/`filter`/`fold`) | Planned | Enhance Layer 3 algorithms with iterator adapters |
| TLS-based error stack (instead of per-call parameter) | Under consideration | Improve ergonomics for deep call chains |
| Windows IOCP backend for eventloop | Planned | Cross-platform event loop completion |
| Benchmarking suite | TBD | Performance measurement against baseline implementations |

---

## 9. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 2.0.0 | 2026-07-29 | Cobalt Dev | Initial baselining release reflecting full 8-layer architecture |
| 1.0.0 | TBD | TBD | Preliminary proposal (internal draft) |

---

## 10. Acknowledgments

Design influenced by COM-style interface modeling, Go's empty interface philosophy applied statically, and modern C11 standards. Special thanks to the C++ Core Guidelines for lessons on safe RAII-like patterns in manual memory-management languages.
