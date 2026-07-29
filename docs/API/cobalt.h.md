# SPEC: API Reference (Layer 6-8 Public API)

**Module:** Public API surface for Layer 6 (Object), Layer 7 (Memory/Runtime), Layer 8 (Platform)

## Overview

This document documents the stable public API that applications and extension modules may directly call. These are the "cornerstone" interfaces defined in core/object.h, core/class.h, core/interface.h, memory/allocator.h, memory/arena.h, runtime/error.h, runtime/logger.h, platform/platform.h, and platform/atomic.h.

## Stability Guarantees

- **Source stability**: Function signatures, parameter types, and return values will not change between minor releases (2.x → 2.y) without deprecation warnings
- **ABI stability**: For shared library builds, all exported symbols use C linkage with stable vtable layouts; forward/backward compatible across patch releases within the same major version
- **No hidden side effects**: All functions are explicitly documented as either allocating or not; reference-counting semantics are explicit via `*_ref`/`*_unpair` pairs

## Core Object API

### Object Lifecycle

| Function | Signature | Behavior |
|----------|-----------|----------|
| `cobalt_object_new` | `cobalt_object_t *cobalt_object_new(cobalt_class_t *cls, size_t extra_size)` | Allocates object with ref_count=1, class pointer set, returns NULL on OOM |
| `cobalt_object_ref` | `void cobalt_object_ref(cobalt_object_t *obj)` | Increments ref_count; no-op if obj is NULL |
| `cobalt_object_unref` | `void cobalt_object_unref(cobalt_object_t *obj)` | Decrements; when count reaches 0, calls destroy callback registered with class (if any) then frees memory |
| `cobalt_object_get_class` | `cobalt_class_t *cobalt_object_get_class(cobalt_object_t *obj)` | Returns NULL if obj is NULL, else returns associated class metadata |

### Class Metadata Queries

| Function | Signature | Behavior |
|----------|-----------|----------|
| `cobalt_object_is_instance_of` | `int cobalt_object_is_instance_of(cobalt_object_t *obj, cobalt_class_t *cls)` | Checks if obj's class (or any ancestor) matches cls — supports single-inheritance hierarchy check |
| `cobalt_object_query_interface` | `void *cobalt_object_query_interface(cobalt_object_t *obj, const char *iface_name)` | If obj implements named interface, returns pointer to its vtable; otherwise NULL |

### Class Management

| Function | Signature | Behavior |
|----------|-----------|----------|
| `cobalt_class_create` | `cobalt_class_t *cobalt_class_create(const char *name, cobalt_class_t *base_class)` | Creates new class with given name; base_class may be NULL for root class. Returns NULL on OOM. |
| `cobalt_class_add_method` | `int cobalt_class_add_method(cobalt_class_t *cls, const char *name, void *(*invoke)(...))` | Registers a method; returns 0 on success, -1 on duplicate name or NULL args |
| `cobalt_class_destroy` | `void cobalt_class_destroy(cobalt_class_t *cls)` | Frees class及其associated methods; must not be called while objects of this class still exist |

## Memory Subsystem API

### Allocator Interface

The `cobalt_allocator_t` vtable is the standard allocation contract. The following factory functions produce concrete allocator implementations:

| Function | Signature | Description |
|----------|-----------|-------------|
| `cobalt_allocator_get_system` | `cobalt_allocator_t *cobalt_allocator_get_system()` | Returns pointer to system (malloc/free) allocator — static, never freed |
| `cobalt_arena_create` | `cobalt_arena_t *cobalt_arena_create(size_t initial_size)` | Creates arena allocator with pre-allocated buffer; uses system allocator internally for the buffer itself |

Arena-specific operations:

| Function | Signature | Description |
|----------|-----------|-------------|
| `cobalt_arena_alloc` | `void *cobalt_arena_alloc(cobalt_arena_t *arena, size_t size)` | Bump-pointer allocation; grows arena if needed using realloc |
| `cobalt_arena_reset` | `void cobalt_arena_reset(cobalt_arena_t *arena)` | Sets used pointer back to zero; all allocations become invalid until re-allocated |
| `cobalt_arena_destroy` | `void cobalt_arena_destroy(cobalt_arena_t *arena)` | Frees the arena buffer itself; arena becomes unusable thereafter |

### Error Injection Pattern

When allocating memory through an allocator that fails (returns NULL), the error code should be stored via `cobalt_error_set(&err, COBALT_ERROR_OUT_OF_MEMORY)` before returning from the calling function.

## Runtime Services API

### Error Handling

| Function | Signature | Description |
|----------|-----------|-------------|
| `cobalt_error_get_message` | `const char *cobalt_error_get_message(cobalt_error_t code)` | Maps error code to descriptive string; never returns NULL |
| `cobalt_error_set` | `void cobalt_error_set(cobalt_error_t *err_ptr, cobalt_error_t code)` | Writes code to caller-provided storage; err_ptr may be NULL (no-op) |
| `cobalt_error_get_current` | `cobalt_error_t cobalt_error_get_current(void)` | Returns thread-local last error; implemented as COBALT_SUCCESS in v2.0 (placeholder for future TLS support) |

### Logging

| Function | Signature | Description |
|----------|-----------|-------------|
| `cobalt_logger_init` | `void cobalt_logger_init(FILE *output_file, log_level_t min_level)` | Initializes logger; output_file may be NULL (defaults to stdout); min_level filters verbosity |
| `cobalt_logger_log` | `void cobalt_logger_log(log_level_t level, const char *file, int line, const char *format, ...)` | Low-level logging with varargs; macro wrappers inject file/line automatically |

Macro levels (lowest to highest): TRACE < DEBUG < INFO < WARNING < ERROR < FATAL

Messages emitted at or above `min_level` are written to `output_file` with format: `[LEVEL] file:line: formatted_message`

FATAL level triggers `exit(1)` immediately after logging.

## Platform Abstraction API

| Function | Signature | Description |
|----------|-----------|-------------|
| `cobalt_platform_get_id` | `cobalt_platform_id_t cobalt_platform_get_id()` | Returns identifier: 1 = Windows, 2 = macOS, 3 = Linux |
| `cobalt_platform_get_handle` | `cobalt_platform_handle_t cobalt_platform_get_handle()` | Opaque platform handle; currently NULL (placeholder for future backend context) |

Atomic operations:

| Function | Signature | Description |
|----------|-----------|-------------|
| `cobalt_atomic_create` | `cobalt_atomic_t cobalt_atomic_create(int initial)` | Returns initialized atomic with given value |
| `cobalt_atomic_get` | `int cobalt_atomic_get(cobalt_atomic_t *a)` | Reads current value (acquire semantics) |
| `cobalt_atomic_set` | `void cobalt_atomic_set(cobalt_atomic_t *a, int value)` | Writes value (release semantics) |
| `cobalt_atomic_increment` | `void cobalt_atomic_increment(cobalt_atomic_t *a)` | Atomic fetch-and-add by 1 |
| `cobalt_atomic_decrement` | `void cobalt_atomic_decrement(cobalt_atomic_t *a)` | Atomic fetch-and-subtract by 1 |

All atomics guarantee lock-free operation on target hardware (verified via `__ATOMIC_*` compiler builtins).

## Header Inclusion Order Recommendation

For maximum compatibility and minimal transitive dependencies, include in this order:

```c
#include <cobalt/platform/platform.h>   /* First: defines platform-specific types */
#include <cobalt/platform/atomic.h>     /* Depends on platform.h */
#include <cobalt/memory/allocator.h>    /* Depends on stddef/stdint */
#include <cobalt/memory/arena.h>        /* Depends on allocator.h */
#include <cobalt/runtime/error.h>       /* Defines error codes used elsewhere */
#include <cobalt/runtime/logger.h>      /* Depends on error.h */
#include <cobalt/core/object.h>         /* Foundation of OO layer */
#include <cobalt/core/class.h>          /* Depends on object.h */
#include <cobalt/core/interface.h>      /* Depends on object.h */
/* ... then Layer 5+ headers as needed */
```

This ordering ensures forward declarations are resolved correctly and prevents circular includes.