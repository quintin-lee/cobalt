/**
 * @file json_parse.c
 * @brief Implementation of the JSON parser
 * @note This file is typically included and compiled by json.c, not exposed separately.
 */
#include "cobalt/module/json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief JSON parsing context
 * Stores the string currently being parsed, position, and total length.
 */
typedef struct {
    const char *str; /**< The JSON string to be parsed */
    int         pos; /**< The current character position being parsed */
    int         len; /**< Total length of the string */
} json_parse_ctx_t;

static inline int is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void json_skip_whitespace(json_parse_ctx_t *ctx)
{
    while (ctx->pos < ctx->len && is_space((unsigned char)ctx->str[ctx->pos])) {
        ctx->pos++;
    }
}

static json_node_t *json_node_create(json_type_t type)
{
    json_node_t *node = malloc(sizeof(json_node_t));
    if (node) {
        node->type = type;
        memset(&node->value, 0, sizeof(node->value));
        node->next = NULL;
        node->key  = NULL;
    }
    return node;
}

static char *json_parse_string(json_parse_ctx_t *ctx)
{
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '"') {
        return NULL;
    }
    ctx->pos++;

    size_t capacity = 64;
    size_t len = 0;
    char *result = malloc(capacity);
    if (!result) {
        return NULL;
    }

    while (ctx->pos < ctx->len && ctx->str[ctx->pos] != '"') {
        if (ctx->str[ctx->pos] == '\\') {
            ctx->pos++;
            if (ctx->pos >= ctx->len) {
                free(result);
                return NULL;
            }
            char c = ctx->str[ctx->pos];
            char escaped[5] = {0};
            size_t elen = 0;

            switch (c) {
            case '"':  escaped[0] = '"'; elen = 1; break;
            case '\\': escaped[0] = '\\'; elen = 1; break;
            case '/':  escaped[0] = '/'; elen = 1; break;
            case 'b':  escaped[0] = '\b'; elen = 1; break;
            case 'f':  escaped[0] = '\f'; elen = 1; break;
            case 'n':  escaped[0] = '\n'; elen = 1; break;
            case 'r':  escaped[0] = '\r'; elen = 1; break;
            case 't':  escaped[0] = '\t'; elen = 1; break;
            case 'u': {
                if (ctx->pos + 4 >= ctx->len) {
                    free(result);
                    return NULL;
                }
                char hex[5] = {0};
                strncpy(hex, ctx->str + ctx->pos + 1, 4);
                unsigned int cp = (unsigned int)strtol(hex, NULL, 16);
                if (cp < 0x80) {
                    escaped[0] = (char)cp;
                    elen = 1;
                } else if (cp < 0x800) {
                    escaped[0] = (char)(0xC0 | (cp >> 6));
                    escaped[1] = (char)(0x80 | (cp & 0x3F));
                    elen = 2;
                } else {
                    escaped[0] = (char)(0xE0 | (cp >> 12));
                    escaped[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    escaped[2] = (char)(0x80 | (cp & 0x3F));
                    elen = 3;
                }
                ctx->pos += 4;
                break;
            }
            default:
                escaped[0] = c;
                elen = 1;
                break;
            }

            if (len + elen + 1 > capacity) {
                capacity = (len + elen + 1) * 2;
                char *tmp = realloc(result, capacity);
                if (!tmp) {
                    free(result);
                    return NULL;
                }
                result = tmp;
            }
            memcpy(result + len, escaped, elen);
            len += elen;
            ctx->pos++;
        } else {
            if (len + 2 > capacity) {
                capacity *= 2;
                char *tmp = realloc(result, capacity);
                if (!tmp) {
                    free(result);
                    return NULL;
                }
                result = tmp;
            }
            result[len++] = ctx->str[ctx->pos++];
        }
    }

    if (ctx->pos >= ctx->len) {
        free(result);
        return NULL;
    }
    ctx->pos++;
    result[len] = '\0';
    return result;
}

static json_node_t *json_parse_value(json_parse_ctx_t *ctx);

static json_node_t *json_parse_object(json_parse_ctx_t *ctx)
{
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '{') {
        return NULL;
    }
    ctx->pos++;

    json_node_t *root = json_node_create(JSON_OBJECT);
    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}') {
        ctx->pos++;
        return root;
    }

    while (1) {
        json_skip_whitespace(ctx);
        if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '"') {
            break;
        }

        char *key = json_parse_string(ctx);
        if (!key) {
            break;
        }

        json_skip_whitespace(ctx);
        if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != ':') {
            free(key);
            break;
        }
        ctx->pos++;

        json_node_t *value = json_parse_value(ctx);
        if (!value) {
            free(key);
            break;
        }

        json_node_t *kv = json_node_create(JSON_OBJECT);
        kv->key         = key;
        kv->next        = value;
        if (!root->next) {
            root->next = kv;
        } else {
            json_node_t *tail = root->next;
            while (tail->next) {
                tail = tail->next;
            }
            tail->next = kv;
        }

        json_skip_whitespace(ctx);
        if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ',') {
            ctx->pos++;
            continue;
        }
        if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}') {
            ctx->pos++;
        }
        break;
    }

    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}') {
        ctx->pos++;
    }
    return root;
}

static json_node_t *json_parse_array(json_parse_ctx_t *ctx)
{
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '[') {
        return NULL;
    }
    ctx->pos++;

    json_node_t *root = json_node_create(JSON_ARRAY);
    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ']') {
        ctx->pos++;
        return root;
    }

    while (1) {
        json_node_t *elem = json_parse_value(ctx);
        if (!elem) {
            break;
        }

        elem->next = root->next;
        root->next = elem;

        json_skip_whitespace(ctx);
        if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ',') {
            ctx->pos++;
            continue;
        }
        break;
    }

    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ']') {
        ctx->pos++;
    }
    return root;
}

/*
 * @brief Attempt to parse any valid JSON value
 *
 * Determines the type based on the current character and dispatches to specific parsing functions.
 */
static json_node_t *json_parse_value(json_parse_ctx_t *ctx)
{
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len) {
        return NULL;
    }

    char c = ctx->str[ctx->pos];

    if (c == '{') {
        return json_parse_object(ctx);
    }
    if (c == '[') {
        return json_parse_array(ctx);
    }
    if (c == '"') {
        char *s = json_parse_string(ctx);
        if (!s) {
            return NULL;
        }
        json_node_t *node  = json_node_create(JSON_STRING);
        node->value.string = s;
        return node;
    }

    if (c == '-' || isdigit((unsigned char)c)) {
        int start = ctx->pos;
        // Simple handling: skip all digits, minus sign, decimal point
        while (ctx->pos < ctx->len &&
               (isdigit((unsigned char)ctx->str[ctx->pos]) || ctx->str[ctx->pos] == '.')) {
            ctx->pos++;
        }
        int   num_len = ctx->pos - start;
        char *num_str = malloc(num_len + 1);
        if (!num_str) {
            return NULL;
        }
        strncpy(num_str, ctx->str + start, num_len);
        num_str[num_len] = '\0';
        double val       = atof(num_str);
        free(num_str);

        json_node_t *node  = json_node_create(JSON_NUMBER);
        node->value.number = val;
        return node;
    }

    if (strncmp(ctx->str + ctx->pos, "true", 4) == 0) {
        ctx->pos += 4;
        return json_node_create(JSON_TRUE);
    }
    if (strncmp(ctx->str + ctx->pos, "false", 5) == 0) {
        ctx->pos += 5;
        return json_node_create(JSON_FALSE);
    }
    if (strncmp(ctx->str + ctx->pos, "null", 4) == 0) {
        ctx->pos += 4;
        return json_node_create(JSON_NULL);
    }

    return NULL;
}

/*
 * @brief Exposed entry point for parsing
 *
 * Wraps the text, initializes the parsing context, and calls json_parse_value.
 */
json_node_t *json_parse(const char *text)
{
    if (!text) {
        return NULL;
    }
    int len = strlen(text);
    if (len == 0) {
        return NULL;
    }

    json_parse_ctx_t ctx = {.str = text, .pos = 0, .len = len};
    json_skip_whitespace(&ctx);
    if (ctx.pos >= ctx.len) {
        return NULL;
    }

    return json_parse_value(&ctx);
}
