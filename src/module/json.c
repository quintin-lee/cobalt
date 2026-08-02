#include "cobalt/module/json.h"
#include "json_parse.c"
#include "json_serialize.c"

double json_get_number(json_node_t *node)
{
    if (node && node->type == JSON_NUMBER) {
        return node->value.number;
    }
    return 0.0;
}

const char *json_get_string(json_node_t *node)
{
    if (node && node->type == JSON_STRING && node->value.string) {
        return node->value.string;
    }
    return "";
}

int json_is_null(json_node_t *node)
{
    return node && node->type == JSON_NULL;
}

int json_is_object(json_node_t *node)
{
    return node && node->type == JSON_OBJECT;
}

int json_is_array(json_node_t *node)
{
    return node && node->type == JSON_ARRAY;
}

json_node_t *json_tree_get_child(json_node_t *parent, const char *key)
{
    if (!parent || !key || parent->type != JSON_OBJECT) {
        return NULL;
    }

    json_node_t *kv = parent->next;
    while (kv) {
        if (kv->key && strcmp(kv->key, key) == 0) {
            return kv->next;
        }
        kv = kv->next ? kv->next->next : NULL;
    }
    return NULL;
}

void json_destroy(json_node_t *node)
{
    if (!node) {
        return;
    }

    if (node->type == JSON_OBJECT || node->type == JSON_ARRAY) {
        json_node_t *child = node->next;
        while (child) {
            json_node_t *value   = child->next;
            json_node_t *next_kv = value ? value->next : NULL;

            if (value && value->type == JSON_STRING && value->value.string) {
                free(value->value.string);
                value->value.string = NULL;
            }
            free(value);
            value = NULL;

            if (child->key) {
                free(child->key);
                child->key = NULL;
            }
            free(child);
            child = next_kv;
        }
        node->next = NULL;
    }

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
