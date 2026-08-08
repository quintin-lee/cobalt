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
 * @brief String duplication implementation
 * @details Calculates the length of the original string, allocates the corresponding memory, and
 * safely copies the contents into the new memory.
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

int cobalt_contains(const char *str, const char *sub)
{
    if (!str || !sub) {
        return 0;
    }
    return strstr(str, sub) != NULL;
}

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
