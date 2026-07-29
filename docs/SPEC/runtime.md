# SPEC: Runtime Subsystem (Layer 7b)

**Module:** `include/cobalt/runtime/`, `src/runtime/`  
**Files:** error.h, error.c, logger.h, logger.c, error.c, logger.c

## 1. Overview

Runtime services provide diagnostic and error-handling capabilities independent of core object semantics. These utilities are used throughout the framework for reporting conditions and debugging.

## 2. Error Handling

### 2.1 Error Code Enumeration

```c
typedef enum {
    COBALT_SUCCESS          = 0,
    COBALT_ERROR_GENERAL    = -1,
    COBALT_ERROR_INVALID_ARGUMENT = -2,
    COBALT_ERROR_OUT_OF_MEMORY  = -3,
    COBALT_ERROR_NOT_FOUND      = -4,
    COBALT_ERROR_ALREADY_EXISTS = -5,
    COBALT_ERROR_PERMISSION_DENIED = -6,
    COBALT_ERROR_IO             = -7,
    COBALT_ERROR_TIMEOUT        = -8,
} cobalt_error_t;
```

### 2.2 API

| Function | Description |
|----------|-------------|
| `cobalt_error_get_message(code)` | Returns human-readable string for error code |
| `cobalt_error_set(err_ptr, code)` | Sets caller-provided error variable |
| `cobalt_error_get_current()` | Returns last error from thread-local storage (future) |

Errors are typically returned as function return values. Optional error-out parameters allow detailed diagnostics:

```c
cobalt_error_t err;
cobalt_handle_t handle = cobalt_platform_create(&err);
if (err != COBALT_SUCCESS) {
    cobalt_logger_log(LOG_ERROR, __FILE__, __LINE__,
                      "Failed to create platform: %s", 
                      cobalt_error_get_message(err));
}
```

### 2.3 Error Stack (Future Enhancement)

Consider adding a push/pop error stack for deep call chains, avoiding manual propagation of every error code.

## 3. Logging Facility

### 3.1 Log Levels

| Level | Macro | Severity |
|-------|-------|----------|
| TRACE | `cobalt_trace()` | Detailed entry/exit tracing |
| DEBUG | `cobalt_debug()` | Debug-time diagnostics |
| INFO  | `cobalt_info()`  | General operational messages |
| WARN  | `cobalt_warning()` | Non-fatal warnings |
| ERROR | `cobalt_error()` | Recoverable errors |
| FATAL | `cobalt_fatal()` | Fatal condition (terminates process) |

### 3.2 API

```c
// Initialize logging subsystem (called early in program start-up)
void cobalt_logger_init(FILE *output_file, log_level_t min_level);

// Low-level log with format string (varargs)
void cobalt_logger_log(log_level_t level, const char *file, int line,
                       const char *format, ...);

// Convenience macros (automatically inject file, line)
#define cobalt_trace(...) cobalt_logger_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_debug(...) cobalt_logger_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_info(...)  cobalt_logger_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_warning(...) cobalt_logger_log(LOG_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_error(...) cobalt_logger_log(LOG_ERROR,  __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_fatal(...) cobalt_logger_log(LOG_FATAL,   __FILE__, __LINE__, __VA_ARGS__)
```

Each logged message includes: `[LEVEL] file:line: formatted_message`

When level equals FATAL, the function calls `exit(1)` automatically.

### 3.3 Configuration

Log output stream and minimum severity level can be changed dynamically via `cobalt_logger_init()`. This permits redirection to files, sockets, or suppression of verbose levels in production builds.
