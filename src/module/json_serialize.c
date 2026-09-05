/**
 * @file json_serialize.c
 * @brief Implementation of the JSON serializer
 */
#include "cobalt/memory/allocator.h"
#include "cobalt/module/json.h"

/**
 * @brief Opaque node structure — definition kept private to JSON module
 */
struct json_node {
    json_type_t       type;
    json_value_t      value;
    struct json_node *next;
    char             *key;
};
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Grow the output buffer so it holds at least @p need bytes
 * @return 0 on success (or when already large enough), -1 on allocation failure
 */
static int ensure_capacity(char **buf, size_t *cap, size_t need, cobalt_allocator_t *alloc)
{
    if (need > *cap) {
        size_t new_cap = (*cap + need) * 2;
        char  *tmp     = cobalt_allocator_realloc(alloc, *buf, new_cap);
        if (!tmp) {
            return -1;
        }
        *buf = tmp;
        *cap = new_cap;
    }
    return 0;
}

/**
 * @brief Append printf-formatted output to the buffer, growing it as needed
 * @return 0 on success, -1 on format or allocation failure
 */
static int
json_append(char **buf, size_t *cap, size_t *len, cobalt_allocator_t *alloc, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (n < 0) {
        return -1;
    }

    if (ensure_capacity(buf, cap, *len + n + 1, alloc) != 0) {
        return -1;
    }

    va_start(args, fmt);
    n = vsnprintf(*buf + *len, *cap - *len, fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= *cap - *len) {
        if (ensure_capacity(buf, cap, *len + n + 1, alloc) != 0) {
            return -1;
        }
        va_start(args, fmt);
        n = vsnprintf(*buf + *len, *cap - *len, fmt, args);
        va_end(args);
    }
    *len += n;
    return 0;
}

/** @brief Duplicate a string with the given allocator; NULL on failure. */
static char *jstrdup(cobalt_allocator_t *alloc, const char *s)
{
    size_t len = strlen(s) + 1;
    char  *dup = cobalt_allocator_alloc(alloc, len);
    if (dup) {
        memcpy(dup, s, len);
    }
    return dup;
}

/**
 * @brief Escape a raw string into its JSON representation
 * @param s Raw bytes to escape (may contain quotes, backslashes, controls)
 * @param len Number of bytes to read from @p s
 * @param alloc Allocator for the escaped output; NULL on allocation failure
 * @return Newly allocated escaped string, NULL on allocation failure
 */
static char *json_escape(const char *s, size_t len, cobalt_allocator_t *alloc)
{
    size_t out = 0;
    for (const char *p = s; p < s + len; p++) {
        switch (*p) {
        case '"':
        case '\\':
        case '\n':
        case '\r':
        case '\t':
            out += 2;
            break;
        default:
            out += ((unsigned char)*p < 0x20) ? 6 : 1;
            break;
        }
    }

    char *result = cobalt_allocator_alloc(alloc, out + 1);
    if (!result) {
        return NULL;
    }

    char *out_ptr = result;
    for (const char *p = s; p < s + len; p++) {
        switch (*p) {
        case '"':
            *out_ptr++ = '\\';
            *out_ptr++ = '"';
            break;
        case '\\':
            *out_ptr++ = '\\';
            *out_ptr++ = '\\';
            break;
        case '\n':
            *out_ptr++ = '\\';
            *out_ptr++ = 'n';
            break;
        case '\r':
            *out_ptr++ = '\\';
            *out_ptr++ = 'r';
            break;
        case '\t':
            *out_ptr++ = '\\';
            *out_ptr++ = 't';
            break;
        default:
            if ((unsigned char)*p < 0x20) {
                out_ptr += sprintf(out_ptr, "\\u00%02x", (unsigned char)*p);
            } else {
                *out_ptr++ = *p;
            }
            break;
        }
    }
    *out_ptr = '\0';
    return result;
}

/**
 * @brief Shared serialization worker used by both public entry points
 * @param node Subtree to serialize (NULL yields "null")
 * @param alloc Resolved allocator, never NULL (callers resolve NULL to system)
 * @return Newly allocated JSON string; "{}" fallback when the buffer itself fails
 */
static char *json_serialize_impl(json_node_t *node, cobalt_allocator_t *alloc)
{
    if (!node) {
        return jstrdup(alloc, "null");
    }

    size_t cap = 256;
    size_t len = 0;
    char  *buf = cobalt_allocator_alloc(alloc, cap);
    if (!buf) {
        return jstrdup(alloc, "{}");
    }

    switch (node->type) {
    case JSON_NULL:
        json_append(&buf, &cap, &len, alloc, "null");
        break;

    case JSON_TRUE:
        json_append(&buf, &cap, &len, alloc, "true");
        break;

    case JSON_FALSE:
        json_append(&buf, &cap, &len, alloc, "false");
        break;

    case JSON_NUMBER:
        json_append(&buf, &cap, &len, alloc, "%.17g", node->value.number);
        break;

    case JSON_STRING: {
        if (node->value.string) {
            char *escaped = json_escape(node->value.string, strlen(node->value.string), alloc);
            if (!escaped) {
                cobalt_allocator_free(alloc, buf);
                return jstrdup(alloc, "{}");
            }
            json_append(&buf, &cap, &len, alloc, "\"%s\"", escaped);
            cobalt_allocator_free(alloc, escaped);
        } else {
            json_append(&buf, &cap, &len, alloc, "\"\"");
        }
        break;
    }

    case JSON_ARRAY: {
        json_append(&buf, &cap, &len, alloc, "[");
        json_node_t *child = node->next;
        int          first = 1;
        while (child) {
            if (!first) {
                json_append(&buf, &cap, &len, alloc, ",");
            }
            first   = 0;
            char *s = json_serialize_impl(child, alloc);
            if (s) {
                json_append(&buf, &cap, &len, alloc, "%s", s);
                cobalt_allocator_free(alloc, s);
            }
            child = child->next;
        }
        json_append(&buf, &cap, &len, alloc, "]");
        break;
    }

    case JSON_OBJECT: {
        json_append(&buf, &cap, &len, alloc, "{");
        json_node_t *kv    = node->next;
        int          first = 1;
        while (kv) {
            if (!first) {
                json_append(&buf, &cap, &len, alloc, ",");
            }
            first = 0;
            char *key_esc =
                json_escape(kv->key ? kv->key : "", strlen(kv->key ? kv->key : ""), alloc);
            if (key_esc) {
                json_append(&buf, &cap, &len, alloc, "\"%s\":", key_esc);
                cobalt_allocator_free(alloc, key_esc);
            }
            char *val = json_serialize_impl(kv->next, alloc);
            if (val) {
                json_append(&buf, &cap, &len, alloc, "%s", val);
                cobalt_allocator_free(alloc, val);
            }
            kv = kv->next ? kv->next->next : NULL;
        }
        json_append(&buf, &cap, &len, alloc, "}");
        break;
    }
    }

    json_append(&buf, &cap, &len, alloc, "");
    return buf;
}

char *json_serialize_with_alloc(json_node_t *node, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }
    return json_serialize_impl(node, alloc);
}

/**
 * @brief Serialize a JSON node tree with the system allocator
 * @param node Root node of the JSON node tree
 * @return Dynamically allocated JSON string (see json_serialize_with_alloc)
 */
char *json_serialize(json_node_t *node)
{
    return json_serialize_with_alloc(node, NULL);
}
