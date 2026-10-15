/**
 * @file json_serialize.c
 * @brief Optimized implementation of the JSON serializer
 */
#include "cobalt/memory/allocator.h"
#include "cobalt/module/json.h"
#include "json_internal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Dynamic string buffer for single-pass geometric expansion
 */
typedef struct {
    char               *data;
    size_t              cap;
    size_t              len;
    cobalt_allocator_t *alloc;
    bool                oom;
} json_buf_t;

static inline bool buf_ensure(json_buf_t *b, size_t needed)
{
    if (b->oom) {
        return false;
    }
    if (b->len + needed + 1 > b->cap) {
        size_t new_cap = (b->cap + needed + 1) * 2;
        if (new_cap < 512) {
            new_cap = 512;
        }
        char *tmp = (char *)cobalt_allocator_realloc(b->alloc, b->data, new_cap);
        if (!tmp) {
            b->oom = true;
            return false;
        }
        b->data = tmp;
        b->cap  = new_cap;
    }
    return true;
}

static inline void buf_append_char(json_buf_t *b, char c)
{
    if (buf_ensure(b, 1)) {
        b->data[b->len++] = c;
    }
}

static inline void buf_append_str(json_buf_t *b, const char *s, size_t len)
{
    if (len == 0) {
        return;
    }
    if (buf_ensure(b, len)) {
        memcpy(b->data + b->len, s, len);
        b->len += len;
    }
}

static inline void buf_append_number(json_buf_t *b, double val)
{
    char tmp[64];
    int  n = snprintf(tmp, sizeof(tmp), "%.17g", val);
    if (n > 0) {
        buf_append_str(b, tmp, (size_t)n);
    }
}

/**
 * @brief Fast escaped string appender avoiding intermediate heap allocation
 */
static void buf_append_escaped_string(json_buf_t *b, const char *s, size_t len)
{
    buf_append_char(b, '"');

    /* Fast scan: check if any byte needs escaping */
    bool needs_escape = false;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c == '"' || c == '\\' || c < 0x20) {
            needs_escape = true;
            break;
        }
    }

    if (!needs_escape) {
        /* Direct copy for clean ASCII and UTF-8 strings */
        buf_append_str(b, s, len);
    } else {
        /* Escape characters directly into output buffer */
        for (size_t i = 0; i < len; i++) {
            uint8_t c = (uint8_t)s[i];
            switch (c) {
            case '"':
                buf_append_str(b, "\\\"", 2);
                break;
            case '\\':
                buf_append_str(b, "\\\\", 2);
                break;
            case '\n':
                buf_append_str(b, "\\n", 2);
                break;
            case '\r':
                buf_append_str(b, "\\r", 2);
                break;
            case '\t':
                buf_append_str(b, "\\t", 2);
                break;
            case '\b':
                buf_append_str(b, "\\b", 2);
                break;
            case '\f':
                buf_append_str(b, "\\f", 2);
                break;
            default:
                if (c < 0x20) {
                    char hex_buf[8];
                    snprintf(hex_buf, sizeof(hex_buf), "\\u00%02x", c);
                    buf_append_str(b, hex_buf, 6);
                } else {
                    buf_append_char(b, (char)c);
                }
                break;
            }
            if (b->oom) {
                return;
            }
        }
    }

    buf_append_char(b, '"');
}

/**
 * @brief Recursive node serialization directly into the growing output buffer
 */
static void serialize_node(json_node_t *node, json_buf_t *b)
{
    if (b->oom) {
        return;
    }
    if (!node) {
        buf_append_str(b, "null", 4);
        return;
    }

    switch (node->type) {
    case JSON_NULL:
        buf_append_str(b, "null", 4);
        break;

    case JSON_TRUE:
        buf_append_str(b, "true", 4);
        break;

    case JSON_FALSE:
        buf_append_str(b, "false", 5);
        break;

    case JSON_NUMBER:
        buf_append_number(b, node->value.number);
        break;

    case JSON_STRING:
        if (node->value.string) {
            buf_append_escaped_string(b, node->value.string, strlen(node->value.string));
        } else {
            buf_append_str(b, "\"\"", 2);
        }
        break;

    case JSON_ARRAY: {
        buf_append_char(b, '[');
        json_node_t *elem  = node->child ? node->child : node->next;
        bool         first = true;
        while (elem && !b->oom) {
            if (!first) {
                buf_append_char(b, ',');
            }
            first = false;
            serialize_node(elem, b);
            elem = elem->next;
        }
        buf_append_char(b, ']');
        break;
    }

    case JSON_OBJECT: {
        buf_append_char(b, '{');
        json_node_t *kv    = node->child ? node->child : node->next;
        bool         first = true;
        while (kv && !b->oom) {
            if (!first) {
                buf_append_char(b, ',');
            }
            first = false;

            const char *key = kv->key ? kv->key : "";
            buf_append_escaped_string(b, key, strlen(key));
            buf_append_char(b, ':');

            json_node_t *val = kv->child ? kv->child : kv->next;
            serialize_node(val, b);
            kv = kv->child ? kv->next : (kv->next ? kv->next->next : NULL);
        }
        buf_append_char(b, '}');
        break;
    }
    }
}

static char *jstrdup(cobalt_allocator_t *alloc, const char *s)
{
    size_t len = strlen(s) + 1;
    char  *dup = (char *)cobalt_allocator_alloc(alloc, len);
    if (dup) {
        memcpy(dup, s, len);
    }
    return dup;
}

char *json_serialize_with_alloc(json_node_t *node, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }

    if (!node) {
        return jstrdup(alloc, "null");
    }

    json_buf_t b = {
        .data  = NULL,
        .cap   = 0,
        .len   = 0,
        .alloc = alloc,
        .oom   = false,
    };

    /* Initial capacity estimation */
    if (!buf_ensure(&b, 256)) {
        return jstrdup(alloc, "{}");
    }

    serialize_node(node, &b);

    if (b.oom || !b.data) {
        if (b.data) {
            cobalt_allocator_free(alloc, b.data);
        }
        return jstrdup(alloc, "{}");
    }

    b.data[b.len] = '\0';
    return b.data;
}

char *json_serialize(json_node_t *node)
{
    return json_serialize_with_alloc(node, NULL);
}
