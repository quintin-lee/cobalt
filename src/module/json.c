#include "cobalt/module/json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================
   JSON NODE HELPERS
   ============================================================ */

static json_node_t* json_node_create(json_type_t type) {
    json_node_t *node = malloc(sizeof(json_node_t));
    if (node) {
        node->type = type;
        node->value = (json_value_t){0};
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
   PARSER CONTEXT
   ============================================================ */

typedef struct {
    const char *str;
    int pos;
    int len;
} json_parse_ctx_t;

static inline int is_space(int c) {
    return c == ' ' || c == '\\t' || c == '\\n' || c == '\\r';
}

static void json_skip_whitespace(json_parse_ctx_t *ctx) {
    while (ctx->pos < ctx->len && is_space((unsigned char)ctx->str[ctx->pos])) {
        ctx->pos++;
    }
}

/* Parse string - simplistic but works for basic cases */
static char* json_parse_string(json_parse_ctx_t *ctx, int consume_quote) {
    if (consume_quote && ctx->pos >= ctx->len || ctx->str[ctx->pos] != '"') {
        return NULL;
    }
    if (consume_quote) ctx->pos++; // skip opening quote
    
    int start = ctx->pos;
    while (ctx->pos < ctx->len) {
        char c = ctx->str[ctx->pos];
        if (c == '"') {
            ctx->pos++;
            break;
        }
        if (c == '\\') {
            ctx->pos += 2; // simple escape skip
            continue;
        }
        ctx->pos++;
    }
    
    if (ctx->pos > start) {
        int len = ctx->pos - start;
        char *result = malloc(len + 1);
        if (result) {
            strncpy(result, ctx->str + start, len);
            result[len] = '\\0';
            return result;
        }
    }
    return NULL;
}

/* ============================================================
   RECURSIVE DESCENT PARSE
   ============================================================ */

static json_node_t* json_parse_value(json_parse_ctx_t *ctx);

static json_node_t* json_parse_object(json_parse_ctx_t *ctx) {
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '{') return NULL;
    ctx->pos++; // skip {
    
    json_node_t *root = json_node_create(JSON_OBJECT);
    root->key = strdup("object");
    
    while (1) {
        json_skip_whitespace(ctx);
        if (ctx->pos >= ctx->len) break;
        if (ctx->str[ctx->pos] == '}') {
            ctx->pos++;
            break;
        }
        
        // Expect string key
        char *key = json_parse_string(ctx, 1);
        if (!key) break;
        
        json_skip_whitespace(ctx);
        if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != ':') {
            free(key);
            break;
        }
        ctx->pos++; // skip :
        
        json_node_t *val = json_parse_value(ctx);
        if (!val) {
            free(key);
            break;
        }
        
        // Create a pair node
        json_node_t *pair = json_node_create(JSON_OBJECT);
        pair->key = key;
        pair->value.number = 0; // marker
        // We store the value in the 'next' chain of the object
        // For simplicity, just append to root's next list
        pair->next = root->next;
        root->next = pair;
        
        json_skip_whitespace(ctx);
        if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ',') {
            ctx->pos++;
            continue;
        }
        break;
    }
    
    return root;
}

static json_node_t* json_parse_array(json_parse_ctx_t *ctx) {
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '[') return NULL;
    ctx->pos++; // skip [
    
    json_node_t *root = json_node_create(JSON_ARRAY);
    root->key = strdup("array");
    
    while (1) {
        json_skip_whitespace(ctx);
        if (ctx->pos >= ctx->len) break;
        if (ctx->str[ctx->pos] == ']') {
            ctx->pos++;
            break;
        }
        
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
    
    return root;
}

static json_node_t* json_parse_value(json_parse_ctx_t *ctx) {
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len) return NULL;
    
    char c = ctx->str[ctx->pos];
    
    if (c == '{') return json_parse_object(ctx);
    if (c == '[') return json_parse_array(ctx);
    if (c == '"') {
        char *s = json_parse_string(ctx, 1);
        if (!s) return NULL;
        json_node_t *node = json_node_create(JSON_STRING);
        node->value.string = s;
        return node;
    }
    
    // number: simple digit parse with optional decimal
    if (c == '-' || isdigit((unsigned char)c)) {
        int start = ctx->pos;
        while (ctx->pos < ctx->len && (isdigit((unsigned char)ctx->str[ctx->pos]) || ctx->str[ctx->pos] == '.')) {
            ctx->pos++;
        }
        int num_len = ctx->pos - start;
        char *num_str = malloc(num_len + 1);
        if (!num_str) return NULL;
        strncpy(num_str, ctx->str + start, num_len);
        num_str[num_len] = '\\0';
        double val = atof(num_str);
        free(num_str);
        
        json_node_t *node = json_node_create(JSON_NUMBER);
        node->value.number = val;
        return node;
    }
    
    // booleans and null
    if (strncmp(ctx->str + ctx->pos, "true", 4) == 0) {
        ctx->pos += 4;
        json_node_t *node = json_node_create(JSON_TRUE);
        return node;
    }
    if (strncmp(ctx->str + ctx->pos, "false", 5) == 0) {
        ctx->pos += 5;
        json_node_t *node = json_node_create(JSON_FALSE);
        return node;
    }
    if (strncmp(ctx->str + ctx->pos, "null", 4) == 0) {
        ctx->pos += 4;
        json_node_t *node = json_node_create(JSON_NULL);
        return node;
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
    return json_parse_value(&ctx);
}

char *json_serialize(json_node_t *node) {
    // Placeholder: return "{}" for now
    // Full implementation would traverse the tree and generate proper JSON
    (void)node;
    char *s = malloc(3);
    if (s) {
        s[0] = '{';
        s[1] = '}';
        s[2] = '\\0';
    }
    return s;
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
    return node && (node->type == JSON_OBJECT || node->type == JSON_ARRAY);
}

int json_is_array(json_node_t *node) {
    return node && node->type == JSON_ARRAY;
}
