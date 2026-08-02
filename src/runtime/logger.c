/**
 * @file logger.c
 * @brief Implementation file of the logging framework
 * @details Implements the log formatting, filtering, and output functions defined in logger.h.
 */
#include "cobalt/runtime/logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Internal log output file pointer, undefined by default before initialization, set via the init
 * function */
static FILE *log_output;
/* Global minimum log level, logs below this level will not be output, default is INFO */
static log_level_t min_log_level = LOG_LEVEL_INFO;

/**
 * @brief Initialize the logging system
 *
 * @param output_file Specifies the file stream for log output; if NULL is passed, standard output
 * stdout is used by default
 * @param min_level   Specifies the minimum log filtering level
 */
void cobalt_logger_init(FILE *output_file, log_level_t min_level)
{
    log_output    = output_file ? output_file : stdout;
    min_log_level = min_level;
}

/**
 * @brief Convert the log level enumeration value to its corresponding string name
 *
 * @param level Log level enumeration
 * @return Corresponding all-uppercase string name, or "UNKNOWN" if the level is unknown
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
 * @brief Log and output formatted logs
 * @details If the current log level is lower than the set minimum level, return immediately.
 *          After formatting the output, if the log level is FATAL, exit(1) will be automatically
 * called to terminate the program.
 *
 * @param level  Current log level
 * @param file   Source code filename
 * @param line   Line number in the source code
 * @param format Formatted message string
 * @param ...    Variable argument list
 */
void cobalt_logger_log(log_level_t level, const char *file, int line, const char *format, ...)
{
    /* Level filtering: if the current level is lower than the set minimum level, ignore this log */
    if (level < min_log_level) {
        return;
    }

    va_list args;
    va_start(args, format);

    /* Print log prefix: [Level] Filename:Line: */
    fprintf(log_output, "[%s] %s:%d: ", level_name(level), file, line);
    /* Print actual formatted message */
    vfprintf(log_output, format, args);
    /* Add newline character */
    fprintf(log_output, "\n");

    va_end(args);

    /* Terminate program execution upon encountering a fatal error */
    if (level == LOG_LEVEL_FATAL) {
        exit(1);
    }
}