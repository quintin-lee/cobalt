#API Reference — Platform(Layer 8)

**Module : ** `include / cobalt / platform /`

    ##Overview

        The platform layer abstracts OS and hardware differences,
    providing platform detection, atomic operations,
    and handle types.This is the foundation layer upon which all other layers depend.

        ##Platform Detection

```c
#include "cobalt/platform/platform.h"

        typedef enum {
            COBALT_PLATFORM_UNKNOWN = 0,
            COBALT_PLATFORM_WINDOWS = 1,
            COBALT_PLATFORM_MACOS   = 2,
            COBALT_PLATFORM_LINUX   = 3,
            COBALT_PLATFORM_OTHER   = 99,
        } cobalt_platform_id_t;
```

    ## #Functions

    | Function | Description | | -- -- -- -- --| -- -- -- -- -- -- -|
    | `cobalt_platform_get_id()` | Returns platform enum(compile - time detection) |
    | `cobalt_platform_get_handle()` | Returns opaque platform handle(NULL if unavailable) |

    ## #Compile - Time Macros

```c
#ifdef _WIN32
#define COBALT_PLATFORM_WINDOWS 1
#elif __APPLE__
#define COBALT_PLATFORM_MACOS 1
#else
#define COBALT_PLATFORM_LINUX 1
#endif
```

        ##Atomic Operations

```c
#include "cobalt/platform/atomic.h"

        typedef struct cobalt_atomic {
    _Atomic int value;
} cobalt_atomic_t;
```

### Functions

| Function | Description |
|----------|-------------|
| `cobalt_atomic_create(initial)` | Create and initialize atomic variable |
| `cobalt_atomic_get(a)` | Read value (memory_order_acquire) |
| `cobalt_atomic_set(a, val)` | Write value (memory_order_release) |
| `cobalt_atomic_increment(a)` | Atomic increment (memory_order_relaxed) |
| `cobalt_atomic_decrement(a)` | Atomic decrement (memory_order_relaxed) |

All operations use C11 `<stdatomic.h>`. On supported architectures, ref-count updates are LOCK-FREE.

## Future: Alignment & Endian

Planned additions:
- `cobalt_align(value, boundary)` — portable aligned allocation helper
- `cobalt_host_to_le32()`, `cobalt_le32_to_host()` — cross-platform endian conversion
- Thread primitives: mutex, condition variable, thread-local storage
