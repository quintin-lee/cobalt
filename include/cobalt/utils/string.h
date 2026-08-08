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

/**
 * @brief Check if a string starts with a given prefix
 * @param str The string to check
 * @param prefix The prefix to look for
 * @return 1 if str starts with prefix, 0 otherwise
 * @note NULL inputs return 0
 */
int cobalt_starts_with(const char *str, const char *prefix);

/**
 * @brief Check if a string ends with a given suffix
 * @param str The string to check
 * @param suffix The suffix to look for
 * @return 1 if str ends with suffix, 0 otherwise
 * @note NULL inputs return 0
 */
int cobalt_ends_with(const char *str, const char *suffix);

/**
 * @brief Check if a string contains a given substring
 * @param str The string to search in
 * @param sub The substring to search for
 * @return 1 if str contains sub, 0 otherwise
 * @note NULL inputs return 0
 */
int cobalt_contains(const char *str, const char *sub);

/** @} */

#endif /* COBALT_STRING_UTIL_H */
