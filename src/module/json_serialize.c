/**
 * @file json_serialize.c
 * @brief Implementation of the JSON serializer
 * @note This file is typically included and compiled by json.c, not exposed separately.
 */
#include "cobalt/module/json.h"
#include "cobalt/utils/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * @brief Apply JSON escaping to a string
 * @param str The original string to escape
 * @param len The length of the string
 * @return Dynamically allocated, escaped string (must be manually freed), returns NULL on failure
 *
 * Two passes: the first pass calculates the required length after escaping, and the second pass
 * performs the actual escaping and writing.
 */
static char *json_escape_string(const char *str, size_t len)
{
    size_t      out_len = 0;
    const char *p       = str;
    const char *end     = str + len;

    while (p < end) {
        char c = *p++;
        switch (c) {
        case '"':
            out_len += 2;
            break;
        case '\\':
            out_len += 2;
            break;
        case '\n':
            out_len += 2;
            break;
        case '\r':
            out_len += 2;
            break;
        case '\t':
            out_len += 2;
            break;
        default:
            if ((unsigned char)c < 0x20) {
                out_len += 6;
            } else {
                out_len += 1;
            }
            break;
        }
    }

    char *result = malloc(out_len + 1);
    if (!result) {
        return NULL;
    }

    p         = str;
    char *out = result;
    while (p < end) {
        char c = *p++;
        switch (c) {
        case '"':
            *out++ = '\\';
            *out++ = '"';
            break;
        case '\\':
            *out++ = '\\';
            *out++ = '\\';
            break;
        case '\n':
            *out++ = '\\';
            *out++ = 'n';
            break;
        case '\r':
            *out++ = '\\';
            *out++ = 'r';
            break;
        case '\t':
            *out++ = '\\';
            *out++ = 't';
            break;
        default:
            if ((unsigned char)c < 0x20) {
                out += sprintf(out, "\\u00%02x", (unsigned char)c);
            } else {
                *out++ = c;
            }
            break;
        }
    }
    *out = '\0';
    return result;
}

/*
 * @brief Recursively serialize a JSON node tree to a JSON string
 * @param node The JSON node to serialize
 * @return Dynamically allocated serialized string (caller must free manually), returns "{}" or
 * "null" on failure
 *
 * Converts nodes of different types to strings according to the JSON format specification, using a
 * dynamically expanding buffer for storage.
 */
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
        if (length + 5 > capacity) {
            capacity  = 512;
            char *tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        memcpy(buffer + length, "null", 5);
        length += 4;
        break;
    case JSON_TRUE:
        if (length + 5 > capacity) {
            capacity  = 512;
            char *tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        memcpy(buffer + length, "true", 5);
        length += 4;
        break;
    case JSON_FALSE:
        if (length + 6 > capacity) {
            capacity  = 512;
            char *tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        memcpy(buffer + length, "false", 6);
        length += 5;
        break;
    case JSON_NUMBER:
        length += snprintf(buffer + length, capacity - length, "%.17g", node->value.number);
        if (length >= capacity) {
            capacity  = length + 64;
            char *tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
            length += snprintf(buffer + length, capacity - length, "%.17g", node->value.number);
        }
        break;
    case JSON_STRING:
        if (node->value.string) {
            char *escaped = json_escape_string(node->value.string, strlen(node->value.string));
            if (escaped) {
                length += snprintf(buffer + length, capacity - length, "\"%s\"", escaped);
                if (length >= capacity) {
                    capacity  = length + 128;
                    char *tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(escaped);
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                    length += snprintf(buffer + length, capacity - length, "\"%s\"", escaped);
                }
                free(escaped);
            } else {
                if (length + 3 > capacity) {
                    capacity  = 512;
                    char *tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                memcpy(buffer + length, "\"\"", 3);
                length += 2;
            }
        } else {
            if (length + 3 > capacity) {
                capacity  = 512;
                char *tmp = realloc(buffer, capacity);
                if (!tmp) {
                    free(buffer);
                    return cobalt_strdup("{}");
                }
                buffer = tmp;
            }
            memcpy(buffer + length, "\"\"", 3);
            length += 2;
        }
        break;
    case JSON_ARRAY: {
        if (length + 2 > capacity) {
            capacity  = 512;
            char *tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        memcpy(buffer + length, "[", 2);
        length += 1;
        json_node_t *child = node->next;
        int          first = 1;
        while (child) {
            if (!first) {
                if (length + 2 > capacity) {
                    capacity *= 2;
                    char *tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                buffer[length++] = ',';
            }
            first   = 0;
            char *s = json_serialize(child);
            if (s) {
                size_t slen = strlen(s);
                if (length + slen + 1 > capacity) {
                    capacity  = length + slen + 64;
                    char *tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(s);
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                memcpy(buffer + length, s, slen);
                length += slen;
                free(s);
            }
            child = child->next;
        }
        if (length + 2 > capacity) {
            capacity  = 512;
            char *tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        buffer[length++] = ']';
        break;
    }
    case JSON_OBJECT: {
        if (length + 2 > capacity) {
            capacity  = 512;
            char *tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        memcpy(buffer + length, "{", 2);
        length += 1;
        json_node_t *kv    = node->next;
        int          first = 1;
        while (kv) {
            if (!first) {
                if (length + 2 > capacity) {
                    capacity *= 2;
                    char *tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                buffer[length++] = ',';
            }
            first = 0;
            char *key_escaped =
                json_escape_string(kv->key ? kv->key : "", strlen(kv->key ? kv->key : ""));
            if (key_escaped) {
                size_t klen = strlen(key_escaped);
                if (length + klen + 3 > capacity) {
                    capacity  = length + klen + 128;
                    char *tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(key_escaped);
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                length += snprintf(buffer + length, capacity - length, "\"%s\":", key_escaped);
                free(key_escaped);
            }
            char *val = json_serialize(kv->next);
            if (val) {
                size_t vlen = strlen(val);
                if (length + vlen + 1 > capacity) {
                    capacity  = length + vlen + 64;
                    char *tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(val);
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                memcpy(buffer + length, val, vlen);
                length += vlen;
                free(val);
            }
            kv = kv->next ? kv->next->next : NULL;
        }
        if (length + 2 > capacity) {
            capacity  = 512;
            char *tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        buffer[length++] = '}';
        break;
    }
    }

    buffer[length] = '\0';
    return buffer;
}
