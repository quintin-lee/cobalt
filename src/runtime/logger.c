/**
 * @file logger.c
 * @brief Implementation of the logging framework
 *
 * @details Provides a lightweight, global logging facility with level-based filtering.
 *          The logger maintains two pieces of global state:
 *          - `log_output`: the output stream (stdout by default, or a user-supplied FILE*)
 *          - `min_log_level`: the minimum severity level that will be emitted
 *
 *          Log messages are formatted as `[LEVEL] file:line: message\n`. When a
 *          FATAL-level message is emitted, the process terminates via `exit(1)`.
 */

#include "cobalt/runtime/logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Output stream for all log messages
 *
 * @details Initialized to `stdout` by `cobalt_logger_init()` if the caller passes
 *          NULL. Thread-safe in the sense that `fprintf` is generally thread-safe
 *          on POSIX, but interleaved messages from multiple threads may produce
 *          mixed output. For fully thread-safe logging, external synchronization
 *          or a thread-local buffer strategy is recommended.
 */
static FILE *log_output;

/**
 * @brief Minimum log level; messages below this threshold are suppressed
 *
 * @details Defaults to `LOG_LEVEL_INFO`. Set via `cobalt_logger_init()`. Levels
 *          are ordered: TRACE < DEBUG < INFO < WARNING < ERROR < FATAL.
 */
static log_level_t min_log_level = LOG_LEVEL_INFO;

/* ========================================================================= */
/* Public API                                                                */
/* ========================================================================= */

/**
 * @brief Initialize the logging subsystem
 *
 * @details Sets the global output stream and minimum log level. If `output_file`
 *          is NULL, logging defaults to `stdout`. This function is not thread-safe
 *          with respect to concurrent log emissions — call it during single-threaded
 *          startup before any worker threads begin logging.
 *
 * @param output_file File stream for log output; NULL defaults to stdout
 * @param min_level   Minimum severity level to emit (messages below are dropped)
 */
void cobalt_logger_init(FILE *output_file, log_level_t min_level)
{
    log_output    = output_file ? output_file : stdout;
    min_log_level = min_level;
}

/**
 * @brief Convert a log level enum to its uppercase string representation
 *
 * @details Used internally to format the `[LEVEL]` prefix in log output. Returns
 *          "UNKNOWN" for values outside the defined enum range.
 *
 * @param level Log level enumeration value
 * @return Constant string name (e.g. "INFO", "ERROR"), or "UNKNOWN"
 */
static const char *level_name(log_level_t level)
{
    switch (level) {
    case LOG_LEVEL_TRACE:
        return "TRACE";
    case LOG_LEVEL_DEBUG:
        return "DEBUG";
    case LOG_LEVEL_INFO:
        return "INFO";
    case LOG_LEVEL_WARNING:
        return "WARNING";
    case LOG_LEVEL_ERROR:
        return "ERROR";
    case LOG_LEVEL_FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief Emit a formatted log message
 *
 * @details Performs level-based filtering, then formats the message with a
 *          `[LEVEL] file:line: ` prefix followed by the user-supplied format
 *          string and a trailing newline.
 *
 *          If `level` is `LOG_LEVEL_FATAL`, `exit(1)` is called after the message
 *          is flushed, terminating the process.
 *
 * @param level  Severity level of this message
 * @param file   Source filename (typically `__FILE__`)
 * @param line   Source line number (typically `__LINE__`)
 * @param format printf-style format string
 * @param ...    Variable arguments matching `format`
 */
void cobalt_logger_log(log_level_t level, const char *file, int line, const char *format, ...)
{
    if (level < min_log_level) {
        return;
    }

    va_list args;
    va_start(args, format);

    fprintf(log_output, "[%s] %s:%d: ", level_name(level), file, line);
    vfprintf(log_output, format, args);
    fprintf(log_output, "\n");

    va_end(args);

    if (level == LOG_LEVEL_FATAL) {
        exit(1);
    }
}
