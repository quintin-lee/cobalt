#ifndef LOGGER_H
#define LOGGER_H

/**
 * @file logger.h
 * @brief Simple logging framework
 */

#include <stdint.h>
#include <stdio.h>

/* Log levels */
typedef enum {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
} log_level_t;

/* Log message structure */
typedef struct {
    log_level_t level;
    const char *file;
    int         line;
    const char *message;
} log_msg_t;

/* Initialize logger */
void cobalt_logger_init(FILE *output_file, log_level_t min_level);

/* Log a message */
void cobalt_logger_log(log_level_t level, const char *file, int line, const char *format, ...);

/* Convenience macros */
#define cobalt_trace(...) cobalt_logger_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_debug(...) cobalt_logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_info(...) cobalt_logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_warning(...) cobalt_logger_log(LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_error(...) cobalt_logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define cobalt_fatal(...) cobalt_logger_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif /* LOGGER_H */
