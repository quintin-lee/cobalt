/**
 * @file string.c
 * @brief String utility module implementation
 */

#include "cobalt/utils/string.h"
#include "cobalt/memory/allocator.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static char *strdup_impl(const char *s, cobalt_allocator_t *alloc)
{
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s) + 1;
    char  *dup = (char *)alloc->alloc(alloc, len);
    if (dup) {
        memcpy(dup, s, len);
    }
    return dup;
}

static int vformat_impl(char **out, const char *fmt, va_list ap, cobalt_allocator_t *alloc)
{
    if (!out || !fmt) {
        return -1;
    }
    *out = NULL;

    va_list ap_copy;
    va_copy(ap_copy, ap);
    int n = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (n < 0) {
        return -1;
    }

    char *buf = (char *)alloc->alloc(alloc, (size_t)n + 1U);
    if (!buf) {
        return -1;
    }

    va_list ap_copy2;
    va_copy(ap_copy2, ap);
    int written = vsnprintf(buf, (size_t)n + 1U, fmt, ap_copy2);
    va_end(ap_copy2);
    if (written < 0) {
        alloc->free(alloc, buf);
        return -1;
    }

    *out = buf;
    return written;
}

static char **split_impl(const char *str, char delim, int *count, cobalt_allocator_t *alloc)
{
    if (!str) {
        if (count) {
            *count = 0;
        }
        return NULL;
    }

    int cnt = 1;
    for (const char *p = str; *p; p++) {
        if (*p == delim) {
            cnt++;
        }
    }

    char **parts = (char **)alloc->alloc(alloc, sizeof(char *) * ((size_t)cnt + 1U));
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
            parts[idx] = (char *)alloc->alloc(alloc, len + 1U);
            if (!parts[idx]) {
                for (int i = 0; i < idx; i++) {
                    alloc->free(alloc, parts[i]);
                }
                alloc->free(alloc, parts);
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
    parts[idx] = NULL;
    if (count) {
        *count = idx;
    }
    return parts;
}

static char *join_impl(const char **parts, char delim, cobalt_allocator_t *alloc)
{
    if (!parts) {
        return NULL;
    }

    size_t total = 0;
    int    n     = 0;
    while (parts[n]) {
        total += strlen(parts[n]);
        n++;
    }
    if (n > 0) {
        total += (size_t)(n - 1) * sizeof(char);
    }

    char *result = (char *)alloc->alloc(alloc, total + 1U);
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

static char *strip_impl(const char *str, cobalt_allocator_t *alloc)
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
    while (end > start &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) {
        end--;
    }

    size_t len    = (size_t)(end - start);
    char  *result = (char *)alloc->alloc(alloc, len + 1U);
    if (!result) {
        return NULL;
    }
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

char *cobalt_strdup(const char *s)
{
    return strdup_impl(s, cobalt_allocator_get_system());
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
    return vformat_impl(out, fmt, ap, cobalt_allocator_get_system());
}

int cobalt_snprintf(char **out, const char *fmt, ...)
{
    if (!out || !fmt) {
        return -1;
    }
    va_list ap;
    va_start(ap, fmt);
    int result = vformat_impl(out, fmt, ap, cobalt_allocator_get_system());
    va_end(ap);
    return result;
}

char **cobalt_split(const char *str, char delim, int *count)
{
    return split_impl(str, delim, count, cobalt_allocator_get_system());
}

char *cobalt_join(const char **parts, char delim)
{
    return join_impl(parts, delim, cobalt_allocator_get_system());
}

char *cobalt_strip(const char *str)
{
    return strip_impl(str, cobalt_allocator_get_system());
}

/* -------------------------------------------------------------------------- */
/* _with_alloc variants                                                       */
/* -------------------------------------------------------------------------- */

char *cobalt_strdup_with_alloc(const char *s, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }
    return strdup_impl(s, alloc);
}

int cobalt_vformat_with_alloc(char **out, const char *fmt, va_list ap, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }
    return vformat_impl(out, fmt, ap, alloc);
}

int cobalt_snprintf_with_alloc(char **out, const char *fmt, cobalt_allocator_t *alloc, ...)
{
    if (!out || !fmt) {
        return -1;
    }
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }
    va_list ap;
    va_start(ap, alloc);
    int result = vformat_impl(out, fmt, ap, alloc);
    va_end(ap);
    return result;
}

char **cobalt_split_with_alloc(const char *str, char delim, int *count, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }
    return split_impl(str, delim, count, alloc);
}

char *cobalt_join_with_alloc(const char **parts, char delim, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }
    return join_impl(parts, delim, alloc);
}

char *cobalt_strip_with_alloc(const char *str, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }
    return strip_impl(str, alloc);
}
