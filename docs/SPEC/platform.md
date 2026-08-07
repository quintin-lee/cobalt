# SPEC: Platform Abstraction Layer (Layer 8)

**Module:** `include/cobalt/platform/`, `src/platform/`  
**Files:** platform.h, atomic.h, platform.c, atomic.c

## 1. Overview

Platform abstraction encapsulates OS/hardware differences, providing uniform primitives for all upper layers. This layer ensures Cobalt compiles and runs correctly across Windows, Linux, macOS, and embedded targets.

## 2. Platform Detection

Conditional defines in `platform.h`:

```c
#ifdef _WIN32
    #define COBALT_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define COBALT_PLATFORM_MACOS 1
#else
    #define COBALT_LINUX 1
#endif
```

Functions returning platform ID:

```c
cobalt_platform_id_t cobalt_platform_get_id(void);  // Returns 1/2/3 for Win/macOS/Linux
cobalt_platform_handle_t cobalt_platform_get_handle(void);  // Opaque platform handle
```

## 3. Atomic Operations

Wrapped around `<stdatomic.h>` for C11 compliance:

```c
typedef struct cobalt_atomic {
    int value;  // In real impl: _Atomic int or similar
} cobalt_atomic_t;

cobalt_atomic_t cobalt_atomic_create(int initial);
int       cobalt_atomic_get(cobalt_atomic_t *a);
void      cobalt_atomic_set(cobalt_atomic_t *a, int value);
void      cobalt_atomic_increment(cobalt_atomic_t *a);
void      cobalt_atomic_decrement(cobalt_atomic_t *a);
```

All atomic operations use LOCK-FREE instructions where supported by the target architecture. This is critical for reference-counted object lifecycles in multithreaded contexts.

## 4. Alignment Utilities (Implemented)

Implemented: cobalt_align, cobalt_align_offset, cobalt_is_aligned macros in include/cobalt/platform/utils.h.

## 5. Endian Conversion (Implemented)

Implemented: cobalt_host_to_net16/32/64, cobalt_net_to_host16/32/64, cobalt_swap16/32/64 in include/cobalt/platform/utils.h.

## 6. Direct I/O Support (Planned)

Future extension: direct file descriptor interfaces for high-throughput scenarios, bypassing OS buffering where needed.

## 7. Thread Primitives (Implemented)

Implemented: cobalt_mutex_t, cobalt_cond_t, cobalt_thread_t with pthreads (Unix) and Win32 compatibility. See include/cobalt/platform/thread.h.
