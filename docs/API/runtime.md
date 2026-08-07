#API Reference — Runtime(Layer 7b)

**Module:** `include/cobalt/runtime/`

## Overview

The runtime layer provides error handling and structured logging. These utilities are used throughout the framework for diagnostics and debugging.

## Error Handling

```c
#include "cobalt/runtime/error.h"

typedef enum {
    COBALT_SUCCESS            =  0,
    COBALT_ERROR_GENERAL      = -1,
    COBALT_ERROR_INVALID_ARGUMENT = -2,
    COBALT_ERROR_OUT_OF_MEMORY    = -3,
    COBALT_ERROR_NOT_FOUND        = -4,
    COBALT_ERROR_ALREADY_EXISTS   = -5,
    COBALT_ERROR_PERMISSION_DENIED = -6,
    COBALT_ERROR_IO               = -7,
    COBALT_ERROR_TIMEOUT          = -8,
    COBALT_ERROR_OUT_OF_BOUNDS    = -9,
    COBALT_ERROR_EMPTY_CONTAINER  = -10,
} cobalt_error_t;
```

### Functions

| Function | Description |
|----------|-------------|
| `cobalt_error_get_message(code)` | Returns human-readable string for error code |
| `cobalt_error_set(err_ptr, code)` | Sets caller-provided error variable and thread-local state |
| `cobalt_error_get_current()` | Returns last error from the current thread |

### Usage Pattern

```c
cobalt_error_t err;
cobalt_handle_t handle = cobalt_create_handle(&err);
if (err != COBALT_SUCCESS) {
    cobalt_logger_log(LOG_ERROR, __FILE__, __LINE__, "Failed: %s", cobalt_error_get_message(err));
}
```

    Most container and module functions return error codes directly(0 = success, -1 = failure)
        .Optional                                          error -
    out parameters                                         use `cobalt_error_set()`.

    ##Logging

```c
#include "cobalt/runtime/logger.h"

    typedef enum {
        LOG_LEVEL_TRACE = 0,
        LOG_LEVEL_DEBUG,
        LOG_LEVEL_INFO,
        LOG_LEVEL_WARNING,
        LOG_LEVEL_ERROR,
        LOG_LEVEL_FATAL,
    } log_level_t;
```

    ## #Initialization

```c cobalt_logger_init(stdout, LOG_LEVEL_INFO); // Minimum level: INFO
```

    ## #Convenience Macros

```c cobalt_trace("detail"); // Most verbose
cobalt_debug("debug");
cobalt_info("info");
cobalt_warning("warn");
cobalt_error("error");
cobalt_fatal("fatal"); // Calls exit(1) after logging
```

### Output Format

Each log line includes `[LEVEL] file:line: message`. Log output is thread-safe.

### Configuration

Call `cobalt_logger_init()` again to redirect output or raise the minimum level at runtime. This is useful for production environments where TRACE/DEBUG should be suppressed.
