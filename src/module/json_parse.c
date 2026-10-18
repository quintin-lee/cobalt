/**
 * @file json_parse.c
 * @brief Implementation of the JSON parser
 * @note This file is typically included and compiled by json.c, not exposed separately.
 */
#include "cobalt/memory/allocator.h"
#include "cobalt/memory/arena.h"
#include "cobalt/module/json.h"
#include "json_internal.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief JSON parsing context
 * Stores the string currently being parsed, position, and total length.
 */
typedef struct {
    const char         *str;            /**< The JSON string to be parsed */
    size_t              pos;            /**< The current character position being parsed */
    size_t              len;            /**< Total length of the string */
    cobalt_allocator_t *alloc;          /**< Resolved allocator (never NULL after entry) */
    cobalt_arena_t     *arena;          /**< Backing memory arena (if using arena) */
    bool                owns_arena;     /**< True if parser created and owns the arena */
    size_t              depth;          /**< Current object/array nesting depth */
    bool                depth_exceeded; /**< Set when the nesting cap is hit */
} json_parse_ctx_t;

/** @brief Allocate parser memory via the context's resolved allocator or arena. */
static void *jalloc(json_parse_ctx_t *ctx, size_t size)
{
    if (ctx->arena) {
        return cobalt_arena_alloc(ctx->arena, size);
    }
    return cobalt_allocator_alloc(ctx->alloc, size);
}

/** @brief Release parser memory via the context's resolved allocator. */
static void jfree(json_parse_ctx_t *ctx, void *ptr)
{
    if (!ctx->arena && ptr) {
        cobalt_allocator_free(ctx->alloc, ptr);
    }
}

/** @brief Resize parser memory via the context's resolved allocator or arena. */
static void *jrealloc(json_parse_ctx_t *ctx, void *ptr, size_t new_size)
{
    if (ctx->arena) {
        void *new_ptr = cobalt_arena_alloc(ctx->arena, new_size);
        if (new_ptr && ptr) {
            /* Copy available content from old block */
            memmove(new_ptr, ptr, new_size / 2);
        }
        return new_ptr;
    }
    return cobalt_allocator_realloc(ctx->alloc, ptr, new_size);
}

/**
 * @brief Test whether a character is JSON whitespace.
 *
 * @param c Character to test.
 * @return Nonzero for space, tab, newline or carriage return; zero otherwise.
 */
static inline int is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/**
 * @brief Advance the parser cursor past any JSON whitespace.
 *
 * @param ctx Parser context holding the cursor.
 */
static void json_skip_whitespace(json_parse_ctx_t *ctx)
{
    while (ctx->pos < ctx->len && is_space((unsigned char)ctx->str[ctx->pos])) {
        ctx->pos++;
    }
}

/**
 * @brief Create a zero-initialized node of the given type
 * @param ctx Parse context supplying the allocator
 * @param type Node type to create
 * @return New node on success, NULL on allocation failure
 */
static json_node_t *json_node_create(json_parse_ctx_t *ctx, json_type_t type)
{
    json_node_t *node = (json_node_t *)jalloc(ctx, sizeof(json_node_t));
    if (node) {
        node->type = type;
        memset(&node->value, 0, sizeof(node->value));
        node->next       = NULL;
        node->child      = NULL;
        node->key        = NULL;
        node->arena      = NULL;
        node->owns_arena = false;
    }
    return node;
}

/**
 * @brief Parse a JSON string literal into a heap-allocated C string.
 *
 * @param ctx Parser context positioned at the opening quote.
 * @return Unescaped string, or NULL on malformed input or allocation failure.
 */
static char *json_parse_string(json_parse_ctx_t *ctx)
{
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '"') {
        return NULL;
    }
    ctx->pos++;

    size_t capacity = 64;
    size_t len      = 0;
    char  *result   = (char *)jalloc(ctx, capacity);
    if (!result) {
        return NULL;
    }

    while (ctx->pos < ctx->len && ctx->str[ctx->pos] != '"') {
        if (ctx->str[ctx->pos] == '\\') {
            ctx->pos++;
            if (ctx->pos >= ctx->len) {
                jfree(ctx, result);
                return NULL;
            }
            char   c          = ctx->str[ctx->pos];
            char   escaped[5] = {0};
            size_t elen       = 0;

            switch (c) {
            case '"':
                escaped[0] = '"';
                elen       = 1;
                break;
            case '\\':
                escaped[0] = '\\';
                elen       = 1;
                break;
            case '/':
                escaped[0] = '/';
                elen       = 1;
                break;
            case 'b':
                escaped[0] = '\b';
                elen       = 1;
                break;
            case 'f':
                escaped[0] = '\f';
                elen       = 1;
                break;
            case 'n':
                escaped[0] = '\n';
                elen       = 1;
                break;
            case 'r':
                escaped[0] = '\r';
                elen       = 1;
                break;
            case 't':
                escaped[0] = '\t';
                elen       = 1;
                break;
            case 'u': {
                if (ctx->pos + 4 >= ctx->len) {
                    jfree(ctx, result);
                    return NULL;
                }
                char hex[5] = {0};
                strncpy(hex, ctx->str + ctx->pos + 1, 4);
                unsigned int cp = (unsigned int)strtol(hex, NULL, 16);

                uint32_t unicode = cp;
                if (cp >= 0xD800u && cp <= 0xDBFFu) {
                    /* High surrogate: must be followed by \uDC00-\uDFFF.
                     * Guard: pos+10 < len covers the 4 low digits at pos+7..10. */
                    if (ctx->pos + 10 < ctx->len && ctx->str[ctx->pos + 5] == '\\' &&
                        ctx->str[ctx->pos + 6] == 'u') {
                        char         low_hex[5] = {0};
                        unsigned int low_cp;
                        strncpy(low_hex, ctx->str + ctx->pos + 7, 4);
                        low_cp = (unsigned int)strtol(low_hex, NULL, 16);
                        if (low_cp >= 0xDC00u && low_cp <= 0xDFFFu) {
                            unicode = 0x10000u + (((cp - 0xD800u) << 10) | (low_cp - 0xDC00u));
                            /* Second \uXXXX group spans ctx->pos+5..ctx->pos+10; the
                             * shared post-switch `ctx->pos++` lands past it. */
                            ctx->pos += 10;
                        } else {
                            /* High surrogate with an invalid low half: lone lead. */
                            jfree(ctx, result);
                            return NULL;
                        }
                    } else {
                        jfree(ctx, result);
                        return NULL;
                    }
                } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                    /* Lone low surrogate: no codepoint to represent. */
                    jfree(ctx, result);
                    return NULL;
                } else {
                    ctx->pos += 4;
                }

                if (unicode < 0x80) {
                    escaped[0] = (char)unicode;
                    elen       = 1;
                } else if (unicode < 0x800) {
                    escaped[0] = (char)(0xC0 | (unicode >> 6));
                    escaped[1] = (char)(0x80 | (unicode & 0x3F));
                    elen       = 2;
                } else if (unicode < 0x10000) {
                    escaped[0] = (char)(0xE0 | (unicode >> 12));
                    escaped[1] = (char)(0x80 | ((unicode >> 6) & 0x3F));
                    escaped[2] = (char)(0x80 | (unicode & 0x3F));
                    elen       = 3;
                } else {
                    escaped[0] = (char)(0xF0 | (unicode >> 18));
                    escaped[1] = (char)(0x80 | ((unicode >> 12) & 0x3F));
                    escaped[2] = (char)(0x80 | ((unicode >> 6) & 0x3F));
                    escaped[3] = (char)(0x80 | (unicode & 0x3F));
                    elen       = 4;
                }
                break;
            }
            default:
                escaped[0] = c;
                elen       = 1;
                break;
            }

            if (len + elen + 1 > capacity) {
                size_t need = len > SIZE_MAX - elen - 1 ? SIZE_MAX : len + elen + 1;
                capacity    = need > SIZE_MAX / 2 ? SIZE_MAX : need * 2;
                char *tmp   = (char *)jrealloc(ctx, result, capacity);
                if (!tmp) {
                    jfree(ctx, result);
                    return NULL;
                }
                result = tmp;
            }
            memcpy(result + len, escaped, elen);
            len += elen;
            ctx->pos++;
        } else {
            if (len + 2 > capacity) {
                capacity  = capacity > SIZE_MAX / 2 ? SIZE_MAX : capacity * 2;
                char *tmp = (char *)jrealloc(ctx, result, capacity);
                if (!tmp) {
                    jfree(ctx, result);
                    return NULL;
                }
                result = tmp;
            }
            result[len++] = ctx->str[ctx->pos++];
        }
    }

    if (ctx->pos >= ctx->len) {
        jfree(ctx, result);
        return NULL;
    }
    ctx->pos++;
    result[len] = '\0';
    return result;
}

static json_node_t *json_parse_value(json_parse_ctx_t *ctx);

/**
 * @brief Parse a JSON object into alternating key/value nodes.
 *
 * @param ctx Parser context positioned at the opening brace.
 * @return Object node heading the member chain, or NULL on malformed input or OOM.
 */
static json_node_t *json_parse_object(json_parse_ctx_t *ctx)
{
    if (ctx->depth >= COBALT_JSON_DOM_MAX_DEPTH) {
        ctx->depth_exceeded = true;
        return NULL;
    }
    ctx->depth++;
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '{') {
        return NULL;
    }
    ctx->pos++;

    json_node_t *root = json_node_create(ctx, JSON_OBJECT);
    if (!root) {
        ctx->depth--;
        return NULL;
    }

    json_node_t *tail = NULL;
    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}') {
        ctx->pos++;
        ctx->depth--;
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
            jfree(ctx, key);
            break;
        }
        ctx->pos++;

        json_node_t *value = json_parse_value(ctx);
        if (!value) {
            jfree(ctx, key);
            break;
        }

        json_node_t *kv = json_node_create(ctx, JSON_OBJECT);
        if (!kv) {
            jfree(ctx, key);
            if (!ctx->arena) {
                json_destroy_with_alloc(value, ctx->alloc);
            }
            break;
        }
        kv->key   = key;
        kv->child = value;
        kv->next  = NULL;

        /* O(1) tail append */
        if (!tail) {
            root->child = kv;
            root->next  = kv;
        } else {
            tail->next = kv;
        }
        tail = kv;

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
    ctx->depth--;
    return root;
}

/**
 * @brief Parse a JSON array into a chain of element nodes.
 *
 * @param ctx Parser context positioned at the opening bracket.
 * @return Array node heading the element chain, or NULL on malformed input or OOM.
 */
static json_node_t *json_parse_array(json_parse_ctx_t *ctx)
{
    if (ctx->depth >= COBALT_JSON_DOM_MAX_DEPTH) {
        ctx->depth_exceeded = true;
        return NULL;
    }
    ctx->depth++;
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '[') {
        return NULL;
    }
    ctx->pos++;

    json_node_t *root = json_node_create(ctx, JSON_ARRAY);
    if (!root) {
        ctx->depth--;
        return NULL;
    }

    json_node_t *tail = NULL;
    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ']') {
        ctx->pos++;
        ctx->depth--;
        return root;
    }

    while (1) {
        json_node_t *elem = json_parse_value(ctx);
        if (!elem) {
            break;
        }

        elem->next = NULL;
        if (!tail) {
            root->child = elem;
            root->next  = elem;
        } else {
            tail->next = elem;
        }
        tail = elem;

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
    ctx->depth--;
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
        json_node_t *node = json_node_create(ctx, JSON_STRING);
        if (!node) {
            jfree(ctx, s);
            return NULL;
        }
        node->value.string = s;
        return node;
    }

    if (c == '-' || isdigit((unsigned char)c)) {
        size_t start = ctx->pos;
        if (c == '-') {
            ctx->pos++;
        }
        while (ctx->pos < ctx->len && isdigit((unsigned char)ctx->str[ctx->pos])) {
            ctx->pos++;
        }
        if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '.') {
            ctx->pos++;
            while (ctx->pos < ctx->len && isdigit((unsigned char)ctx->str[ctx->pos])) {
                ctx->pos++;
            }
        }
        if (ctx->pos < ctx->len && (ctx->str[ctx->pos] == 'e' || ctx->str[ctx->pos] == 'E')) {
            ctx->pos++;
            if (ctx->pos < ctx->len && (ctx->str[ctx->pos] == '+' || ctx->str[ctx->pos] == '-')) {
                ctx->pos++;
            }
            while (ctx->pos < ctx->len && isdigit((unsigned char)ctx->str[ctx->pos])) {
                ctx->pos++;
            }
        }

        size_t num_len = ctx->pos - start;
        char  *num_str = (char *)jalloc(ctx, num_len + 1);
        if (!num_str) {
            return NULL;
        }
        strncpy(num_str, ctx->str + start, num_len);
        num_str[num_len] = '\0';
        double val       = strtod(num_str, NULL);
        jfree(ctx, num_str);

        json_node_t *node = json_node_create(ctx, JSON_NUMBER);
        if (!node) {
            return NULL;
        }
        node->value.number = val;
        return node;
    }

    if (ctx->pos + 4 <= ctx->len && strncmp(ctx->str + ctx->pos, "true", 4) == 0) {
        ctx->pos += 4;
        return json_node_create(ctx, JSON_TRUE);
    }
    if (ctx->pos + 5 <= ctx->len && strncmp(ctx->str + ctx->pos, "false", 5) == 0) {
        ctx->pos += 5;
        return json_node_create(ctx, JSON_FALSE);
    }
    if (ctx->pos + 4 <= ctx->len && strncmp(ctx->str + ctx->pos, "null", 4) == 0) {
        ctx->pos += 4;
        return json_node_create(ctx, JSON_NULL);
    }

    return NULL;
}

/* -----------------------------------------------------------------------------
 *  Public DOM Parsing Functions
 * -------------------------------------------------------------------------- */

json_node_t *json_parse_with_arena(const char *text, cobalt_arena_t *arena)
{
    if (!text || !arena) {
        return NULL;
    }
    size_t len = strlen(text);
    if (len == 0) {
        return NULL;
    }

    json_parse_ctx_t ctx = {
        .str            = text,
        .pos            = 0,
        .len            = len,
        .alloc          = NULL,
        .arena          = arena,
        .owns_arena     = false,
        .depth          = 0,
        .depth_exceeded = false,
    };

    json_skip_whitespace(&ctx);
    if (ctx.pos >= ctx.len) {
        return NULL;
    }

    json_node_t *root = json_parse_value(&ctx);
    if (ctx.depth_exceeded) {
        return NULL; /* partial tree, if any, is owned by the caller's arena */
    }
    if (root) {
        root->arena      = arena;
        root->owns_arena = false;
    }
    return root;
}

json_node_t *cobalt_json_parse_with_arena(const char *text, cobalt_arena_t *arena)
{
    return json_parse_with_arena(text, arena);
}

json_node_t *json_parse_with_alloc(const char *text, cobalt_allocator_t *alloc)
{
    if (!text) {
        return NULL;
    }
    size_t len = strlen(text);
    if (len == 0) {
        return NULL;
    }

    cobalt_allocator_t *effective_alloc = alloc ? alloc : cobalt_allocator_get_system();
    cobalt_arena_t     *arena           = cobalt_arena_create_with_allocator(4096, effective_alloc);
    if (!arena) {
        return NULL;
    }

    json_parse_ctx_t ctx = {
        .str            = text,
        .pos            = 0,
        .len            = len,
        .alloc          = effective_alloc,
        .arena          = arena,
        .owns_arena     = true,
        .depth          = 0,
        .depth_exceeded = false,
    };

    json_skip_whitespace(&ctx);
    if (ctx.pos >= ctx.len) {
        cobalt_arena_destroy(arena);
        return NULL;
    }

    json_node_t *root = json_parse_value(&ctx);
    if (!root || ctx.depth_exceeded) {
        cobalt_arena_destroy(arena);
        return NULL;
    }
    root->arena      = arena;
    root->owns_arena = true;
    return root;
}

json_node_t *json_parse(const char *text)
{
    return json_parse_with_alloc(text, NULL);
}
