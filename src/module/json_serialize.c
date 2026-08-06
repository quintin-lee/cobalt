/**
 * @file json_serialize.c
 * @brief Implementation of the JSON serializer
 */
#include "cobalt/module/json.h"

/* Opaque node structure — definition kept private to JSON module */
struct json_node {
    json_type_t       type;
    json_value_t      value;
    struct json_node *next;
    char             *key;
};
#include "cobalt/utils/string.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ensure_capacity(char **buf, size_t *cap, size_t need)
{
    if (need > *cap) {
        size_t new_cap = (*cap + need) * 2;
        char  *tmp     = realloc(*buf, new_cap);
        if (!tmp) {
            return -1;
        }
        *buf = tmp;
        *cap = new_cap;
    }
    return 0;
}

static int json_append(char **buf, size_t *cap, size_t *len, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (n < 0) {
        return -1;
    }

    if (ensure_capacity(buf, cap, *len + n + 1) != 0) {
        return -1;
    }

    va_start(args, fmt);
    n = vsnprintf(*buf + *len, *cap - *len, fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= *cap - *len) {
        if (ensure_capacity(buf, cap, *len + n + 1) != 0) {
            return -1;
        }
        va_start(args, fmt);
        n = vsnprintf(*buf + *len, *cap - *len, fmt, args);
        va_end(args);
    }
    *len += n;
    return 0;
}

static char *json_escape(const char *s, size_t len)
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

    char *result = malloc(out + 1);
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

char *json_serialize(json_node_t *node)
{
    if (!node) {
        return cobalt_strdup("null");
    }

    size_t cap = 256;
    size_t len = 0;
    char  *buf = malloc(cap);
    if (!buf) {
        return cobalt_strdup("{}");
    }

    switch (node->type) {
    case JSON_NULL:
        json_append(&buf, &cap, &len, "null");
        break;

    case JSON_TRUE:
        json_append(&buf, &cap, &len, "true");
        break;

    case JSON_FALSE:
        json_append(&buf, &cap, &len, "false");
        break;

    case JSON_NUMBER:
        json_append(&buf, &cap, &len, "%.17g", node->value.number);
        break;

    case JSON_STRING: {
        if (node->value.string) {
            char *escaped = json_escape(node->value.string, strlen(node->value.string));
            if (!escaped) {
                free(buf);
                return cobalt_strdup("{}");
            }
            json_append(&buf, &cap, &len, "\"%s\"", escaped);
            free(escaped);
        } else {
            json_append(&buf, &cap, &len, "\"\"");
        }
        break;
    }

    case JSON_ARRAY: {
        json_append(&buf, &cap, &len, "[");
        json_node_t *child = node->next;
        int          first = 1;
        while (child) {
            if (!first) {
                json_append(&buf, &cap, &len, ",");
            }
            first   = 0;
            char *s = json_serialize(child);
            if (s) {
                json_append(&buf, &cap, &len, "%s", s);
                free(s);
            }
            child = child->next;
        }
        json_append(&buf, &cap, &len, "]");
        break;
    }

    case JSON_OBJECT: {
        json_append(&buf, &cap, &len, "{");
        json_node_t *kv    = node->next;
        int          first = 1;
        while (kv) {
            if (!first) {
                json_append(&buf, &cap, &len, ",");
            }
            first         = 0;
            char *key_esc = json_escape(kv->key ? kv->key : "", strlen(kv->key ? kv->key : ""));
            if (key_esc) {
                json_append(&buf, &cap, &len, "\"%s\":", key_esc);
                free(key_esc);
            }
            char *val = json_serialize(kv->next);
            if (val) {
                json_append(&buf, &cap, &len, "%s", val);
                free(val);
            }
            kv = kv->next ? kv->next->next : NULL;
        }
        json_append(&buf, &cap, &len, "}");
        break;
    }
    }

    json_append(&buf, &cap, &len, "");
    return buf;
}
