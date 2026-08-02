#ifndef LOGGER_H
#define LOGGER_H

/**
 * @file logger.h
 * @brief Simple logging framework
 * @details Provides leveled log output functionality, can output logs to a specified file stream, and supports formatted strings.
 *
 * @defgroup RuntimeLogger Runtime logging
 * @{
 */

#include <stdint.h>
#include <stdio.h>

/**
 * @brief Log severity level enumeration
 * @details Defines different log levels from TRACE to FATAL, used to control the output detail level.
 */
typedef enum {
    LOG_LEVEL_TRACE = 0, /**< Trace information, the most detailed log level (Trace) */
    LOG_LEVEL_DEBUG,     /**< Debug information (Debug) */
    LOG_LEVEL_INFO,      /**< General hint information (Info) */
    LOG_LEVEL_WARNING,   /**< Warning information, there might be a problem (Warning) */
    LOG_LEVEL_ERROR,     /**< Error information, operation failed but program can continue (Error) */
    LOG_LEVEL_FATAL,     /**< Fatal error, usually causes program termination (Fatal) */
} log_level_t;

/**
 * @brief Log message structure
 * @details Contains the context information of a single log message.
 */
typedef struct {
    log_level_t level;   /**< Severity level of the log */
    const char *file;    /**< Source code filename generating the log */
    int         line;    /**< Source code line number generating the log */
    const char *message; /**< Log content string */
} log_msg_t;

/**
 * @brief Initialize the logging system
 * 
 * @param output_file Target file stream for log output (e.g., stdout, stderr, or a file opened via fopen)
 * @param min_level   The minimum log level allowed to be output; logs below this level will be discarded
 */
void cobalt_logger_init(FILE *output_file, log_level_t min_level);

/**
 * @brief Log a formatted log message
 * @details Filters output based on the current minimum log level, and automatically attaches filename, line number, and other information.
 * 
 * @param level  Severity level of the current log
 * @param file   Current source code filename (usually using __FILE__)
 * @param line   Current source code line number (usually using __LINE__)
 * @param format Format string (similar to printf)
 * @param ...    Variable argument list
 */
void cobalt_logger_log(log_level_t level, const char *file, int line, const char *format, ...);

/**
 * @name Convenient logging macros
 * @brief Logging shortcuts that automatically capture the current filename and line number
 * @{
 */
#define cobalt_trace(...)   cobalt_logger_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_debug(...)   cobalt_logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_info(...)    cobalt_logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_warning(...) cobalt_logger_log(LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_error(...)   cobalt_logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_fatal(...)   cobalt_logger_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)
/** @} */

/** @} */

#endif /* LOGGER_H */
