/**
 * @file string.c
 * @brief String utility module implementation
 */

#include "cobalt/utils/string.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Duplicate a string with heap-allocated storage.
 * @details Calculates the length of the original string, allocates the corresponding memory, and
 * safely copies the contents into the new memory.
 *
 * @param s Source string to duplicate; may be NULL.
 * @return Newly allocated copy, or NULL on NULL input or allocation failure.
 */
char *cobalt_strdup(const char *s)
{
    // Return NULL safely if the input is null
    if (!s) {
        return NULL;
    }

    // Calculate required length (including the terminating '\0')
    size_t len = strlen(s) + 1;

    // Allocate new memory
    char *dup = malloc(len);
    if (dup) {
        // Copy all contents if allocation is successful
        memcpy(dup, s, len);
    }

    return dup;
}

/**
 * @brief Test whether a string begins with a given prefix.
 *
 * @param str String to test; NULL yields false.
 * @param prefix Prefix to look for; NULL yields false.
 * @return Nonzero when str starts with prefix, zero otherwise.
 */
int cobalt_starts_with(const char *str, const char *prefix)
{
    if (!str || !prefix) {
        return 0;
    }
    size_t prefix_len = strlen(prefix);
    if (prefix_len > strlen(str)) {
        return 0;
    }
    return strncmp(str, prefix, prefix_len) == 0;
}

/**
 * @brief Test whether a string ends with a given suffix.
 *
 * @param str String to test; NULL yields false.
 * @param suffix Suffix to look for; NULL yields false.
 * @return Nonzero when str ends with suffix, zero otherwise.
 */
int cobalt_ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix) {
        return 0;
    }
    size_t str_len    = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) {
        return 0;
    }
    return strcmp(str + (str_len - suffix_len), suffix) == 0;
}

/**
 * @brief Test whether a string contains a given substring.
 *
 * @param str String to search; NULL yields false.
 * @param sub Substring to look for; NULL yields false.
 * @return Nonzero when str contains sub, zero otherwise.
 */
int cobalt_contains(const char *str, const char *sub)
{
    if (!str || !sub) {
        return 0;
    }
    return strstr(str, sub) != NULL;
}

/**
 * @brief Format into a heap-allocated string using a va_list.
 *
 * @param out Receives the allocated string on success, NULL otherwise.
 * @param fmt Printf-style format string.
 * @param ap Argument list for the format string.
 * @return Characters written on success, -1 on invalid arguments or failure.
 */
int cobalt_vformat(char **out, const char *fmt, va_list ap)
{
    if (!out || !fmt) {
        return -1;
    }
    *out = NULL;

    /* First pass: determine required buffer size */
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int n = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (n < 0) {
        return -1;
    }

    /* Allocate buffer (n chars + null terminator) */
    char *buf = (char *)malloc((size_t)n + 1U);
    if (!buf) {
        return -1;
    }

    /* Second pass: write into buffer */
    va_list ap_copy2;
    va_copy(ap_copy2, ap);
    int written = vsnprintf(buf, (size_t)n + 1U, fmt, ap_copy2);
    va_end(ap_copy2);
    if (written < 0) {
        free(buf);
        return -1;
    }

    *out = buf;
    return written;
}

/**
 * @brief Format into a heap-allocated string using variadic arguments.
 *
 * @param out Receives the allocated string on success, NULL otherwise.
 * @param fmt Printf-style format string.
 * @return Characters written on success, -1 on invalid arguments or failure.
 */
int cobalt_snprintf(char **out, const char *fmt, ...)
{
    if (!out || !fmt) {
        return -1;
    }
    va_list ap;
    va_start(ap, fmt);
    int result = cobalt_vformat(out, fmt, ap);
    va_end(ap);
    return result;
}

/**
 * @brief Split a string on a delimiter into a NULL-terminated array.
 *
 * @param str String to split; NULL yields NULL with count set to zero.
 * @param delim Delimiter character separating parts.
 * @param count Receives the number of parts; may be NULL.
 * @return NULL-terminated array of heap-allocated parts, or NULL on failure.
 *
 * @note Free each part and then the array itself with free.
 */
char **cobalt_split(const char *str, char delim, int *count)
{
    if (!str) {
        if (count) {
            *count = 0;
        }
        return NULL;
    }

    /* Count delimiters to determine number of parts */
    int cnt = 1;
    for (const char *p = str; *p; p++) {
        if (*p == delim) {
            cnt++;
        }
    }

    char **parts = (char **)malloc(sizeof(char *) * ((size_t)cnt + 1U));
    if (!parts) {
        if (count) {
            *count = 0;
        }
        return NULL;
    }

    int         idx   = 0;
    const char *start = str;
    for (const char *p = str;; p++) {
        if (*p == delim || *p == '\0') {
            size_t len = (size_t)(p - start);
            parts[idx] = (char *)malloc(len + 1U);
            if (!parts[idx]) {
                /* Free already allocated parts on error */
                for (int i = 0; i < idx; i++) {
                    free(parts[i]);
                }
                free(parts);
                if (count) {
                    *count = 0;
                }
                return NULL;
            }
            memcpy(parts[idx], start, len);
            parts[idx][len] = '\0';
            idx++;
            if (*p == '\0') {
                break;
            }
            start = p + 1;
        }
    }
    parts[idx] = NULL; /* NULL-terminated for iteration convenience */

    if (count) {
        *count = idx;
    }
    return parts;
}

/**
 * @brief Join a NULL-terminated string array with a delimiter.
 *
 * @param parts NULL-terminated array of strings; NULL yields NULL.
 * @param delim Delimiter character inserted between parts.
 * @return Newly allocated joined string, or NULL on NULL input or failure.
 */
char *cobalt_join(const char **parts, char delim)
{
    if (!parts) {
        return NULL;
    }

    /* First pass: compute total length */
    size_t total = 0;
    int    n     = 0;
    while (parts[n]) {
        total += strlen(parts[n]);
        n++;
    }
    if (n > 0) {
        total += (size_t)(n - 1) * sizeof(char); /* delimiters */
    }

    char *result = (char *)malloc(total + 1U);
    if (!result) {
        return NULL;
    }

    char *p = result;
    for (int i = 0; parts[i]; i++) {
        if (i > 0) {
            *p++ = delim;
        }
        size_t slen = strlen(parts[i]);
        memcpy(p, parts[i], slen);
        p += slen;
    }
    *p = '\0';
    return result;
}

/**
 * @brief Copy a string with leading and trailing whitespace removed.
 *
 * @param str String to strip; NULL yields NULL.
 * @return Newly allocated stripped copy, or NULL on NULL input or failure.
 */
char *cobalt_strip(const char *str)
{
    if (!str) {
        return NULL;
    }

    const char *start = str;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
        start++;
    }

    const char *end = start;
    while (*end) {
        end++;
    }
    /* end now points to '\0' */
    /* Move end back past trailing whitespace */
    while (end > start &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) {
        end--;
    }

    size_t len    = (size_t)(end - start);
    char  *result = (char *)malloc(len + 1U);
    if (!result) {
        return NULL;
    }
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}
