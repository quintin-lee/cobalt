#ifndef COBALT_STRING_UTIL_H
#define COBALT_STRING_UTIL_H

/**
 * @file string.h
 * @brief String utility module
 *
 * Provides cross-platform, C11 standard-compatible string operation helper functions.
 */

#include <stdarg.h>
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

/**
 * @brief Format a string into a dynamically allocated buffer
 * @param out Pointer to receive the allocated buffer (caller must free)
 * @param fmt printf-style format string
 * @return Number of characters written (excluding null terminator), or -1 on error
 * @note Equivalent to snprintf but with automatic memory allocation.
 *       NULL inputs or OOM return -1 and set *out to NULL.
 */
int cobalt_snprintf(char **out, const char *fmt, ...);

/**
 * @brief Format a string into a dynamically allocated buffer (va_list variant)
 * @param out Pointer to receive the allocated buffer (caller must free)
 * @param fmt printf-style format string
 * @param ap argument list
 * @return Number of characters written (excluding null terminator), or -1 on error
 */
int cobalt_vformat(char **out, const char *fmt, va_list ap);

/**
 * @brief Split a string by delimiter into dynamically allocated substrings
 * @details Splits @p str at each occurrence of @p delim. The resulting substrings are
 *          dynamically allocated — caller must free each element and the array itself.
 *          Consecutive delimiters produce empty strings. Trailing delimiter produces
 *          an extra empty string.
 * @param str    The string to split (must not be NULL)
 * @param delim  The delimiter character
 * @param count  Pointer to receive the number of substrings (can be NULL)
 * @return A dynamically allocated array of substrings, or NULL on error/empty input.
 *         Caller must free each element and the array. Returns NULL if str is NULL.
 */
char **cobalt_split(const char *str, char delim, int *count);

/**
 * @brief Join an array of strings with a delimiter
 * @details Concatenates all strings in @p parts with @p delim between each pair.
 *          The result is dynamically allocated — caller must free it.
 * @param parts  Array of string pointers (must be non-NULL, terminated by NULL)
 * @param delim  Delimiter character
 * @return Dynamically allocated joined string, or NULL on error
 */
char *cobalt_join(const char **parts, char delim);

/**
 * @brief Remove leading and trailing whitespace from a string
 * @details Allocates a new string with leading/trailing whitespace (space, tab, newline,
 *          carriage return) stripped. Caller must free the returned string.
 * @param str The input string (must not be NULL)
 * @return A newly allocated stripped string, or NULL on error
 */
char *cobalt_strip(const char *str);

/** @} */

#endif /* COBALT_STRING_UTIL_H */
