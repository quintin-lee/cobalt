#ifndef COBALT_STRING_UTIL_H
#define COBALT_STRING_UTIL_H

/**
 * @file string.h
 * @brief String utility module
 *
 * Provides cross-platform, C11 standard-compatible string operation helper functions.
 */

#include <stddef.h>

/**
 * @defgroup StringUtil String Utility Module
 * @{
 */

/**
 * @brief Portable string duplication function (C11)
 * @details Duplicates a null-terminated string. Internally allocates enough memory to hold the
 * copy, the user is responsible for freeing the returned string using free(). This solves the
 * problem of strdup being non-standard on some platforms.
 *
 * @param str_src The source string to duplicate
 * @return A pointer to the newly allocated string copy; returns NULL if the source string is NULL
 * or memory allocation fails
 */
char *cobalt_strdup(const char *str_src);

/** @} */

#endif /* COBALT_STRING_UTIL_H */
