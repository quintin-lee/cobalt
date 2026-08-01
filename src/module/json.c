#include "cobalt/module/json.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Portable strdup for C11 */
static char* my_strdup(const char* s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s) + 1;
    char* dup = malloc(len);
    if (dup)
        memcpy(dup, s, len);
    return dup;
}

/* ============================================================
   JSON NODE HELPERS
   ============================================================ */

static json_node_t* json_node_create(json_type_t type)
{
    json_node_t* node = malloc(sizeof(json_node_t));
    if (node)
        {
            node->type = type;
            memset(&node->value, 0, sizeof(node->value));
            node->next = NULL;
            node->key = NULL;
        }
    return node;
}

void json_destroy(json_node_t* node)
{
    if (!node)
        return;

    if (node->type == JSON_OBJECT || node->type == JSON_ARRAY)
        {
            json_node_t* child = node->next;
            while (child)
                {
                    json_node_t* value = child->next;
                    json_node_t* next_kv = value ? value->next : NULL;

                    if (value && value->type == JSON_STRING && value->value.string)
                        {
                            free(value->value.string);
                            value->value.string = NULL;
                        }
                    free(value);
                    value = NULL;

                    if (child->key)
                        {
                            free(child->key);
                            child->key = NULL;
                        }
                    free(child);
                    child = next_kv;
                }
            node->next = NULL;
        }

    if (node->type == JSON_STRING && node->value.string)
        {
            free(node->value.string);
            node->value.string = NULL;
        }
    if (node->key)
        {
            free(node->key);
            node->key = NULL;
        }

    free(node);
}

/* ============================================================
   SERIALIZATION
   ============================================================ */

/* Escape a string for JSON output */
static char* json_escape_string(const char* str, size_t len)
{
    /* Count required buffer size */
    size_t out_len = 0;
    const char* p = str;
    const char* end = str + len;

    while (p < end)
        {
            char c = *p++;
            switch (c)
                {
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
                        if ((unsigned char)c < 0x20)
                            {
                                out_len += 6; /* \u00XX */
                            }
                        else
                            {
                                out_len += 1;
                            }
                        break;
                }
        }

    char* result = malloc(out_len + 1);
    if (!result)
        return NULL;

    p = str;
    char* out = result;
    while (p < end)
        {
            char c = *p++;
            switch (c)
                {
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
                        if ((unsigned char)c < 0x20)
                            {
                                out += sprintf(out, "\\u00%02x", (unsigned char)c);
                            }
                        else
                            {
                                *out++ = c;
                            }
                        break;
                }
        }
    *out = '\0';
    return result;
}

/* Serialize a JSON node tree to string */
char* json_serialize(json_node_t* node)
{
    if (!node)
        return my_strdup("null");

    /* Use a dynamically resized buffer instead of open_memstream for portability */
    size_t capacity = 256;
    size_t length = 0;
    char* buffer = malloc(capacity);
    if (!buffer)
        return my_strdup("{}");

    switch (node->type)
        {
            case JSON_NULL:
                if (length + 5 > capacity)
                    {
                        capacity = 512;
                        char* tmp = realloc(buffer, capacity);
                        if (!tmp)
                            {
                                free(buffer);
                                return my_strdup("{}");
                            }
                        buffer = tmp;
                    }
                memcpy(buffer + length, "null", 5);
                length += 4;
                break;
            case JSON_TRUE:
                if (length + 5 > capacity)
                    {
                        capacity = 512;
                        char* tmp = realloc(buffer, capacity);
                        if (!tmp)
                            {
                                free(buffer);
                                return my_strdup("{}");
                            }
                        buffer = tmp;
                    }
                memcpy(buffer + length, "true", 5);
                length += 4;
                break;
            case JSON_FALSE:
                if (length + 6 > capacity)
                    {
                        capacity = 512;
                        char* tmp = realloc(buffer, capacity);
                        if (!tmp)
                            {
                                free(buffer);
                                return my_strdup("{}");
                            }
                        buffer = tmp;
                    }
                memcpy(buffer + length, "false", 6);
                length += 5;
                break;
            case JSON_NUMBER:
                length += snprintf(buffer + length, capacity - length, "%.17g", node->value.number);
                if (length >= capacity)
                    {
                        capacity = length + 64;
                        char* tmp = realloc(buffer, capacity);
                        if (!tmp)
                            {
                                free(buffer);
                                return my_strdup("{}");
                            }
                        buffer = tmp;
                        length += snprintf(buffer + length, capacity - length, "%.17g",
                                           node->value.number);
                    }
                break;
            case JSON_STRING:
                if (node->value.string)
                    {
                        char* escaped =
                            json_escape_string(node->value.string, strlen(node->value.string));
                        if (escaped)
                            {
                                length +=
                                    snprintf(buffer + length, capacity - length, "\"%s\"", escaped);
                                if (length >= capacity)
                                    {
                                        capacity = length + 128;
                                        char* tmp = realloc(buffer, capacity);
                                        if (!tmp)
                                            {
                                                free(escaped);
                                                free(buffer);
                                                return my_strdup("{}");
                                            }
                                        buffer = tmp;
                                        length += snprintf(buffer + length, capacity - length,
                                                           "\"%s\"", escaped);
                                    }
                                free(escaped);
                            }
                        else
                            {
                                if (length + 3 > capacity)
                                    {
                                        capacity = 512;
                                        char* tmp = realloc(buffer, capacity);
                                        if (!tmp)
                                            {
                                                free(buffer);
                                                return my_strdup("{}");
                                            }
                                        buffer = tmp;
                                    }
                                memcpy(buffer + length, "\"\"", 3);
                                length += 2;
                            }
                    }
                else
                    {
                        if (length + 3 > capacity)
                            {
                                capacity = 512;
                                char* tmp = realloc(buffer, capacity);
                                if (!tmp)
                                    {
                                        free(buffer);
                                        return my_strdup("{}");
                                    }
                                buffer = tmp;
                            }
                        memcpy(buffer + length, "\"\"", 3);
                        length += 2;
                    }
                break;
            case JSON_ARRAY:
                {
                    if (length + 2 > capacity)
                        {
                            capacity = 512;
                            char* tmp = realloc(buffer, capacity);
                            if (!tmp)
                                {
                                    free(buffer);
                                    return my_strdup("{}");
                                }
                            buffer = tmp;
                        }
                    memcpy(buffer + length, "[", 2);
                    length += 1;
                    json_node_t* child = node->next;
                    int first = 1;
                    while (child)
                        {
                            if (!first)
                                {
                                    if (length + 2 > capacity)
                                        {
                                            capacity *= 2;
                                            char* tmp = realloc(buffer, capacity);
                                            if (!tmp)
                                                {
                                                    free(buffer);
                                                    return my_strdup("{}");
                                                }
                                            buffer = tmp;
                                        }
                                    buffer[length++] = ',';
                                }
                            first = 0;
                            char* s = json_serialize(child);
                            if (s)
                                {
                                    size_t slen = strlen(s);
                                    if (length + slen + 1 > capacity)
                                        {
                                            capacity = length + slen + 64;
                                            char* tmp = realloc(buffer, capacity);
                                            if (!tmp)
                                                {
                                                    free(s);
                                                    free(buffer);
                                                    return my_strdup("{}");
                                                }
                                            buffer = tmp;
                                        }
                                    memcpy(buffer + length, s, slen);
                                    length += slen;
                                    free(s);
                                }
                            child = child->next;
                        }
                    if (length + 2 > capacity)
                        {
                            capacity = 512;
                            char* tmp = realloc(buffer, capacity);
                            if (!tmp)
                                {
                                    free(buffer);
                                    return my_strdup("{}");
                                }
                            buffer = tmp;
                        }
                    buffer[length++] = ']';
                    break;
                }
            case JSON_OBJECT:
                {
                    if (length + 2 > capacity)
                        {
                            capacity = 512;
                            char* tmp = realloc(buffer, capacity);
                            if (!tmp)
                                {
                                    free(buffer);
                                    return my_strdup("{}");
                                }
                            buffer = tmp;
                        }
                    memcpy(buffer + length, "{", 2);
                    length += 1;
                    json_node_t* kv = node->next;
                    int first = 1;
                    while (kv)
                        {
                            if (!first)
                                {
                                    if (length + 2 > capacity)
                                        {
                                            capacity *= 2;
                                            char* tmp = realloc(buffer, capacity);
                                            if (!tmp)
                                                {
                                                    free(buffer);
                                                    return my_strdup("{}");
                                                }
                                            buffer = tmp;
                                        }
                                    buffer[length++] = ',';
                                }
                            first = 0;
                            char* key_escaped = json_escape_string(kv->key ? kv->key : "",
                                                                   strlen(kv->key ? kv->key : ""));
                            if (key_escaped)
                                {
                                    size_t klen = strlen(key_escaped);
                                    if (length + klen + 3 > capacity)
                                        {
                                            capacity = length + klen + 128;
                                            char* tmp = realloc(buffer, capacity);
                                            if (!tmp)
                                                {
                                                    free(key_escaped);
                                                    free(buffer);
                                                    return my_strdup("{}");
                                                }
                                            buffer = tmp;
                                        }
                                    length += snprintf(buffer + length, capacity - length,
                                                       "\"%s\":", key_escaped);
                                    free(key_escaped);
                                }
                            char* val = json_serialize(kv->next);
                            if (val)
                                {
                                    size_t vlen = strlen(val);
                                    if (length + vlen + 1 > capacity)
                                        {
                                            capacity = length + vlen + 64;
                                            char* tmp = realloc(buffer, capacity);
                                            if (!tmp)
                                                {
                                                    free(val);
                                                    free(buffer);
                                                    return my_strdup("{}");
                                                }
                                            buffer = tmp;
                                        }
                                    memcpy(buffer + length, val, vlen);
                                    length += vlen;
                                    free(val);
                                }
                            kv = kv->next ? kv->next->next : NULL;
                        }
                    if (length + 2 > capacity)
                        {
                            capacity = 512;
                            char* tmp = realloc(buffer, capacity);
                            if (!tmp)
                                {
                                    free(buffer);
                                    return my_strdup("{}");
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

/* ============================================================
   PARSER CONTEXT
   ============================================================ */

typedef struct
{
    const char* str;
    int pos;
    int len;
} json_parse_ctx_t;

static inline int is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void json_skip_whitespace(json_parse_ctx_t* ctx)
{
    while (ctx->pos < ctx->len && is_space((unsigned char)ctx->str[ctx->pos]))
        {
            ctx->pos++;
        }
}

/* Parse string value */
static char* json_parse_string(json_parse_ctx_t* ctx)
{
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '"')
        return NULL;
    ctx->pos++; /* skip opening quote */

    /* Find closing quote (handle escapes) */
    int start = ctx->pos;
    while (ctx->pos < ctx->len && ctx->str[ctx->pos] != '"')
        {
            if (ctx->str[ctx->pos] == '\\' && ctx->pos + 1 < ctx->len)
                {
                    ctx->pos += 2; /* skip escaped char */
                }
            else
                {
                    ctx->pos++;
                }
        }

    if (ctx->pos >= ctx->len)
        return NULL; /* unmatched quote */

    int len = ctx->pos - start;
    char* result = malloc(len + 1);
    if (!result)
        return NULL;

    strncpy(result, ctx->str + start, len);
    result[len] = '\0';
    ctx->pos++; /* skip closing quote */

    return result;
}

/* ============================================================
   RECURSIVE DESCENT PARSE
   ============================================================ */

static json_node_t* json_parse_value(json_parse_ctx_t* ctx);

static json_node_t* json_parse_object(json_parse_ctx_t* ctx)
{
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '{')
        return NULL;
    ctx->pos++; /* skip { */

    json_node_t* root = json_node_create(JSON_OBJECT);

    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}')
        {
            ctx->pos++;
            return root;
        }

    while (1)
        {
            json_skip_whitespace(ctx);
            if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '"')
                break;

            char* key = json_parse_string(ctx);
            if (!key)
                break;

            json_skip_whitespace(ctx);
            if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != ':')
                {
                    free(key);
                    break;
                }
            ctx->pos++; /* skip : */

            json_node_t* value = json_parse_value(ctx);
            if (!value)
                {
                    free(key);
                    break;
                }

            json_node_t* kv = json_node_create(JSON_OBJECT);
            kv->key = key;
            kv->next = value;
            if (!root->next)
                {
                    root->next = kv;
                }
            else
                {
                    json_node_t* tail = root->next;
                    while (tail->next)
                        tail = tail->next;
                    tail->next = kv;
                }

            json_skip_whitespace(ctx);
            if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ',')
                {
                    ctx->pos++;
                    continue;
                }
            if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}')
                {
                    ctx->pos++;
                }
            break;
        }

    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}')
        {
            ctx->pos++;
        }

    return root;
}

static json_node_t* json_parse_array(json_parse_ctx_t* ctx)
{
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '[')
        return NULL;
    ctx->pos++; /* skip [ */

    json_node_t* root = json_node_create(JSON_ARRAY);

    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ']')
        {
            ctx->pos++;
            return root;
        }

    while (1)
        {
            json_node_t* elem = json_parse_value(ctx);
            if (!elem)
                break;

            elem->next = root->next;
            root->next = elem;

            json_skip_whitespace(ctx);
            if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ',')
                {
                    ctx->pos++;
                    continue;
                }
            break;
        }

    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ']')
        {
            ctx->pos++;
        }

    return root;
}

static json_node_t* json_parse_value(json_parse_ctx_t* ctx)
{
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len)
        return NULL;

    char c = ctx->str[ctx->pos];

    if (c == '{')
        return json_parse_object(ctx);
    if (c == '[')
        return json_parse_array(ctx);
    if (c == '"')
        {
            char* s = json_parse_string(ctx);
            if (!s)
                return NULL;
            json_node_t* node = json_node_create(JSON_STRING);
            node->value.string = s;
            return node;
        }

    /* number */
    if (c == '-' || isdigit((unsigned char)c))
        {
            int start = ctx->pos;
            while (ctx->pos < ctx->len &&
                   (isdigit((unsigned char)ctx->str[ctx->pos]) || ctx->str[ctx->pos] == '.'))
                {
                    ctx->pos++;
                }
            int num_len = ctx->pos - start;
            char* num_str = malloc(num_len + 1);
            if (!num_str)
                return NULL;
            strncpy(num_str, ctx->str + start, num_len);
            num_str[num_len] = '\0';
            double val = atof(num_str);
            free(num_str);

            json_node_t* node = json_node_create(JSON_NUMBER);
            node->value.number = val;
            return node;
        }

    /* boolean and null */
    if (strncmp(ctx->str + ctx->pos, "true", 4) == 0)
        {
            ctx->pos += 4;
            return json_node_create(JSON_TRUE);
        }
    if (strncmp(ctx->str + ctx->pos, "false", 5) == 0)
        {
            ctx->pos += 5;
            return json_node_create(JSON_FALSE);
        }
    if (strncmp(ctx->str + ctx->pos, "null", 4) == 0)
        {
            ctx->pos += 4;
            return json_node_create(JSON_NULL);
        }

    return NULL;
}

/* ============================================================
   CHILD LOOKUP
   ============================================================ */

json_node_t* json_tree_get_child(json_node_t* parent, const char* key)
{
    if (!parent || !key || parent->type != JSON_OBJECT)
        return NULL;

    /* Children are stored as key-value pairs: kv->next = value */
    json_node_t* kv = parent->next;
    while (kv)
        {
            if (kv->key && strcmp(kv->key, key) == 0)
                {
                    return kv->next; /* return the value node */
                }
            kv = kv->next ? kv->next->next : NULL; /* skip to next key */
        }
    return NULL;
}

/* ============================================================
   PUBLIC API
   ============================================================ */

json_node_t* json_parse(const char* text)
{
    if (!text)
        return NULL;
    int len = strlen(text);
    if (len == 0)
        return NULL;

    json_parse_ctx_t ctx = {.str = text, .pos = 0, .len = len};
    json_skip_whitespace(&ctx);
    if (ctx.pos >= ctx.len)
        return NULL;

    return json_parse_value(&ctx);
}

double json_get_number(json_node_t* node)
{
    if (node && node->type == JSON_NUMBER)
        return node->value.number;
    return 0.0;
}

const char* json_get_string(json_node_t* node)
{
    if (node && node->type == JSON_STRING && node->value.string)
        return node->value.string;
    return "";
}

int json_is_null(json_node_t* node)
{
    return node && node->type == JSON_NULL;
}

int json_is_object(json_node_t* node)
{
    return node && node->type == JSON_OBJECT;
}

int json_is_array(json_node_t* node)
{
    return node && node->type == JSON_ARRAY;
}
