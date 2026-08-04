/**
 * @file json_serialize.c
 * @brief Implementation of the JSON serializer
 */
#include "cobalt/module/json.h"
#include "cobalt/utils/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static int ensure_capacity(char **buffer, size_t *capacity, size_t needed)
{
    if (needed > *capacity - *buffer + (*capacity - needed)) {
        size_t new_cap = (*capacity + needed) * 2;
        char  *tmp     = realloc(*buffer, new_cap);
        if (!tmp) {
            return -1;
        }
        *buffer   = tmp;
        *capacity = new_cap;
    }
    return 0;
}

static int json_append_str(char **buf, size_t *cap, size_t *len, const char *s, size_t n)
{
    if (ensure_capacity(buf, cap, n + 1) != 0) {
        return -1;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    return 0;
}

static int json_append_fmt(char **buf, size_t *cap, size_t *len, const char *fmt, ...)
{
    if (ensure_capacity(buf, cap, 256) != 0) {
        return -1;
    }
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(*buf + *len, *cap - *len, fmt, args);
    va_end(args);
    if (written < 0 || (size_t)written >= *cap - *len) {
        size_t needed = *len + written + 1;
        if (ensure_capacity(buf, cap, needed) != 0) {
            return -1;
        }
        va_start(args, fmt);
        vsnprintf(*buf + *len, *cap - *len, fmt, args);
        va_end(args);
    }
    *len += strlen(*buf + *len);
    return 0;
}

static char *json_escape_string(const char *str, size_t len)
{
    size_t out_len = 0;
    for (const char *p = str; p < str + len; p++) {
        switch (*p) {
        case '"': case '\\': case '\n': case '\r': case '\t':
            out_len += 2;
            break;
        default:
            out_len += ((unsigned char)*p < 0x20) ? 6 : 1;
            break;
        }
    }

    char *result = malloc(out_len + 1);
    if (!result) {
        return NULL;
    }

    char *out = result;
    for (const char *p = str; p < str + len; p++) {
        switch (*p) {
        case '"':  *out++ = '\\'; *out++ = '"';  break;
        case '\\': *out++ = '\\'; *out++ = '\\'; break;
        case '\n': *out++ = '\\'; *out++ = 'n';  break;
        case '\r': *out++ = '\\'; *out++ = 'r';  break;
        case '\t': *out++ = '\\'; *out++ = 't';  break;
        default:
            if ((unsigned char)*p < 0x20) {
                out += sprintf(out, "\\u00%02x", (unsigned char)*p);
            } else {
                *out++ = *p;
            }
            break;
        }
    }
    *out = '\0';
    return result;
}

char *json_serialize(json_node_t *node)
{
    if (!node) {
        return cobalt_strdup("null");
    }

    size_t capacity = 256;
    size_t length   = 0;
    char  *buffer   = malloc(capacity);
    if (!buffer) {
        return cobalt_strdup("{}");
    }

    switch (node->type) {
    case JSON_NULL:
        json_append_str(&buffer, &capacity, &length, "null", 4);
        break;

    case JSON_TRUE:
        json_append_str(&buffer, &capacity, &length, "true", 4);
        break;

    case JSON_FALSE:
        json_append_str(&buffer, &capacity, &length, "false", 5);
        break;

    case JSON_NUMBER:
        json_append_fmt(&buffer, &capacity, &length, "%.17g", node->value.number);
        break;

    case JSON_STRING: {
        if (node->value.string) {
            char *escaped = json_escape_string(node->value.string, strlen(node->value.string));
            if (!escaped) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            json_append_fmt(&buffer, &capacity, &length, "\"%s\"", escaped);
            free(escaped);
        } else {
            json_append_str(&buffer, &capacity, &length, "\"\"", 2);
        }
        break;
    }

    case JSON_ARRAY: {
        json_append_str(&buffer, &capacity, &length, "[", 1);
        json_node_t *child = node->next;
        int first = 1;
        while (child) {
            if (!first) {
                json_append_str(&buffer, &capacity, &length, ",", 1);
            }
            first = 0;
            char *s = json_serialize(child);
            if (s) {
                size_t slen = strlen(s);
                json_append_str(&buffer, &capacity, &length, s, slen);
                free(s);
            }
            child = child->next;
        }
        json_append_str(&buffer, &capacity, &length, "]", 1);
        break;
    }

    case JSON_OBJECT: {
        json_append_str(&buffer, &capacity, &length, "{", 1);
        json_node_t *kv = node->next;
        int first = 1;
        while (kv) {
            if (!first) {
                json_append_str(&buffer, &capacity, &length, ",", 1);
            }
            first = 0;
            char *key_escaped = json_escape_string(kv->key ? kv->key : "", strlen(kv->key ? kv->key : ""));
            if (key_escaped) {
                size_t klen = strlen(key_escaped);
                json_append_fmt(&buffer, &capacity, &length, "\"%s\":", key_escaped);
                free(key_escaped);
            }
            char *val = json_serialize(kv->next);
            if (val) {
                size_t vlen = strlen(val);
                json_append_str(&buffer, &capacity, &length, val, vlen);
                free(val);
            }
            kv = kv->next ? kv->next->next : NULL;
        }
        json_append_str(&buffer, &capacity, &length, "}", 1);
        break;
    }
    }

    json_append_str(&buffer, &capacity, &length, "", 1);
    return buffer;
}
