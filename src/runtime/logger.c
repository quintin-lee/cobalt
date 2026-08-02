#include "cobalt/runtime/logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE       *log_output; /* Will be initialized by cobalt_logger_init */
static log_level_t min_log_level = LOG_LEVEL_INFO;

void cobalt_logger_init(FILE *output_file, log_level_t min_level)
{
    log_output    = output_file ? output_file : stdout;
    min_log_level = min_level;
}

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