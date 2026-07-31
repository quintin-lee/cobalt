#include "cobalt/module/json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

/* ============================================================
   JSON NODE HELPERS
   ============================================================ */

static json_node_t* json_node_create(json_type_t type) {
    json_node_t *node = malloc(sizeof(json_node_t));
    if (node) {
        node->type = type;
        memset(&node->value, 0, sizeof(node->value));
        node->next = NULL;
        node->key = NULL;
    }
    return node;
}

void json_destroy(json_node_t *node) {
    if (!node) return;
    
    /* Destroy children first */
    json_node_t *child = node->next;
    while (child) {
        json_node_t *next = child->next;
        json_destroy(child);
        child = next;
    }
    
    /* Clean up this node's string values */
    if (node->type == JSON_STRING && node->value.string) {
        free(node->value.string);
        node->value.string = NULL;
    }
    if (node->key) {
        free(node->key);
        node->key = NULL;
    }
    
    free(node);
}

/* ============================================================
   SERIALIZATION
   ============================================================ */

/* Escape a string for JSON output */
static char* json_escape_string(const char *str, size_t len) {
    /* Count required buffer size */
    size_t out_len = 0;
    const char *p = str;
    const char *end = str + len;
    
    while (p < end) {
        char c = *p++;
        switch (c) {
            case '"': out_len += 2; break;
            case '\\': out_len += 2; break;
            case '\n': out_len += 2; break;
            case '\r': out_len += 2; break;
            case '\t': out_len += 2; break;
            default:
                if ((unsigned char)c < 0x20) {
                    out_len += 6; /* \u00XX */
                } else {
                    out_len += 1;
                }
                break;
        }
    }
    
    char *result = malloc(out_len + 1);
    if (!result) return NULL;
    
    p = str;
    char *out = result;
    while (p < end) {
        char c = *p++;
        switch (c) {
            case '"': *out++ = '\\'; *out++ = '"'; break;
            case '\\': *out++ = '\\'; *out++ = '\\'; break;
            case '\n': *out++ = '\\'; *out++ = 'n'; break;
            case '\r': *out++ = '\\'; *out++ = 'r'; break;
            case '\t': *out++ = '\\'; *out++ = 't'; break;
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

/* Serialize a JSON node tree to string */
char *json_serialize(json_node_t *node) {
    if (!node) return strdup("null");
    
    char *result = NULL;
    FILE *fp = open_memstream(&result, NULL);
    if (!fp) return strdup("{}");
    
    switch (node->type) {
        case JSON_NULL:
            fprintf(fp, "null");
            break;
        case JSON_TRUE:
            fprintf(fp, "true");
            break;
        case JSON_FALSE:
            fprintf(fp, "false");
            break;
        case JSON_NUMBER:
            fprintf(fp, "%.17g", node->value.number);
            break;
        case JSON_STRING:
            if (node->value.string) {
                char *escaped = json_escape_string(node->value.string, strlen(node->value.string));
                if (escaped) {
                    fprintf(fp, "\"%s\"", escaped);
                    free(escaped);
                } else {
                    fprintf(fp, "\"\"");
                }
            } else {
                fprintf(fp, "\"\"");
            }
            break;
        case JSON_ARRAY:
            fprintf(fp, "[");
            json_node_t *child = node->next;
            int first = 1;
            while (child) {
                if (!first) fprintf(fp, ",");
                char *s = json_serialize(child);
                if (s) {
                    fprintf(fp, "%s", s);
                    free(s);
                }
                first = 0;
                child = child->next;
            }
            fprintf(fp, "]");
            break;
        case JSON_OBJECT:
            fprintf(fp, "{");
            json_node_t *kv = node->next;
            first = 1;
            while (kv) {
                if (!first) fprintf(fp, ",");
                char *key_escaped = json_escape_string(kv->key ? kv->key : "", strlen(kv->key ? kv->key : ""));
                if (key_escaped) {
                    fprintf(fp, "\"%s\":", key_escaped);
                    free(key_escaped);
                }
                char *val = json_serialize(kv->next); /* value is in next */
                if (val) {
                    fprintf(fp, "%s", val);
                    free(val);
                }
                first = 0;
                kv = kv->next ? kv->next->next : NULL; /* skip to next key */
            }
            fprintf(fp, "}");
            break;
    }
    
    fclose(fp);
    return result;
}

/* ============================================================
   PARSER CONTEXT
   ============================================================ */

typedef struct {
    const char *str;
    int pos;
    int len;
} json_parse_ctx_t;

static inline int is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void json_skip_whitespace(json_parse_ctx_t *ctx) {
    while (ctx->pos < ctx->len && is_space((unsigned char)ctx->str[ctx->pos])) {
        ctx->pos++;
    }
}

/* Parse string value */
static char* json_parse_string(json_parse_ctx_t *ctx) {
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '"') return NULL;
    ctx->pos++; /* skip opening quote */
    
    /* Find closing quote (handle escapes) */
    int start = ctx->pos;
    while (ctx->pos < ctx->len && ctx->str[ctx->pos] != '"') {
        if (ctx->str[ctx->pos] == '\\' && ctx->pos + 1 < ctx->len) {
            ctx->pos += 2; /* skip escaped char */
        } else {
            ctx->pos++;
        }
    }
    
    if (ctx->pos >= ctx->len) return NULL; /* unmatched quote */
    
    int len = ctx->pos - start;
    char *result = malloc(len + 1);
    if (!result) return NULL;
    
    strncpy(result, ctx->str + start, len);
    result[len] = '\0';
    ctx->pos++; /* skip closing quote */
    
    return result;
}

/* ============================================================
   RECURSIVE DESCENT PARSE
   ============================================================ */

static json_node_t* json_parse_value(json_parse_ctx_t *ctx);

static json_node_t* json_parse_object(json_parse_ctx_t *ctx) {
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '{') return NULL;
    ctx->pos++; /* skip { */
    
    json_node_t *root = json_node_create(JSON_OBJECT);
    
    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}') {
        ctx->pos++;
        return root;
    }
    
    while (1) {
        json_skip_whitespace(ctx);
        if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '"') break;
        
        char *key = json_parse_string(ctx);
        if (!key) break;
        
        json_skip_whitespace(ctx);
        if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != ':') {
            free(key);
            break;
        }
        ctx->pos++; /* skip : */
        
        json_node_t *value = json_parse_value(ctx);
        if (!value) {
            free(key);
            break;
        }
        
        /* Add key-value pair to object */
        json_node_t *kv = json_node_create(JSON_OBJECT);
        kv->key = key;
        kv->next = value; /* value stored as next */
        kv->next->next = root->next; /* chain */
        root->next = kv;
        
        json_skip_whitespace(ctx);
        if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ',') {
            ctx->pos++;
            continue;
        }
        break;
    }
    
    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}') {
        ctx->pos++;
    }
    
    return root;
}

static json_node_t* json_parse_array(json_parse_ctx_t *ctx) {
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '[') return NULL;
    ctx->pos++; /* skip [ */
    
    json_node_t *root = json_node_create(JSON_ARRAY);
    
    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ']') {
        ctx->pos++;
        return root;
    }
    
    while (1) {
        json_node_t *elem = json_parse_value(ctx);
        if (!elem) break;
        
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

static json_node_t* json_parse_value(json_parse_ctx_t *ctx) {
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len) return NULL;
    
    char c = ctx->str[ctx->pos];
    
    if (c == '{') return json_parse_object(ctx);
    if (c == '[') return json_parse_array(ctx);
    if (c == '"') {
        char *s = json_parse_string(ctx);
        if (!s) return NULL;
        json_node_t *node = json_node_create(JSON_STRING);
        node->value.string = s;
        return node;
    }
    
    /* number */
    if (c == '-' || isdigit((unsigned char)c)) {
        int start = ctx->pos;
        while (ctx->pos < ctx->len && (isdigit((unsigned char)ctx->str[ctx->pos]) || ctx->str[ctx->pos] == '.')) {
            ctx->pos++;
        }
        int num_len = ctx->pos - start;
        char *num_str = malloc(num_len + 1);
        if (!num_str) return NULL;
        strncpy(num_str, ctx->str + start, num_len);
        num_str[num_len] = '\0';
        double val = atof(num_str);
        free(num_str);
        
        json_node_t *node = json_node_create(JSON_NUMBER);
        node->value.number = val;
        return node;
    }
    
    /* boolean and null */
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

/* ============================================================
   PUBLIC API
   ============================================================ */

json_node_t *json_parse(const char *text) {
    if (!text) return NULL;
    int len = strlen(text);
    if (len == 0) return NULL;
    
    json_parse_ctx_t ctx = { .str = text, .pos = 0, .len = len };
    json_skip_whitespace(&ctx);
    if (ctx.pos >= ctx.len) return NULL;
    
    return json_parse_value(&ctx);
}

double json_get_number(json_node_t *node) {
    if (node && node->type == JSON_NUMBER) return node->value.number;
    return 0.0;
}

const char *json_get_string(json_node_t *node) {
    if (node && node->type == JSON_STRING && node->value.string) return node->value.string;
    return "";
}

int json_is_null(json_node_t *node) {
    return node && node->type == JSON_NULL;
}

int json_is_object(json_node_t *node) {
    return node && node->type == JSON_OBJECT;
}

int json_is_array(json_node_t *node) {
    return node && node->type == JSON_ARRAY;
}
