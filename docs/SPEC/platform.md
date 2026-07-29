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

## 4. Alignment Utilities (Planned)

Future extension: `cobalt_align(value, boundary)` macro/function for portable aligned allocation, important for SIMD instructions and structure packing.

## 5. Endian Conversion (Planned)

Future extension: `cobalt_host_to_le32()`, `le32_to_host()` helpers for cross-platform binary data interchange.

## 6. Direct I/O Support (Planned)

Future extension: direct file descriptor interfaces for high-throughput scenarios, bypassing OS buffering where needed.

## 7. Thread Primitives (Planned)

Future extension: mutex, condition variable, and thread-local storage wrappers above native threading APIs (pthread on Unix, Windows threads on Win32).
