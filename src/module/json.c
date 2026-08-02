/**
 * @file json.c
 * @brief Implementation of JSON query and memory management interfaces
 */
#include "cobalt/module/json.h"
#include "json_parse.c"
#include "json_serialize.c"

/*
 * @brief Safely get the numeric value of a JSON node
 * @return Returns the actual value if it's JSON_NUMBER, otherwise returns 0.0
 */
double json_get_number(json_node_t *node)
{
    if (node && node->type == JSON_NUMBER) {
        return node->value.number;
    }
    return 0.0;
}

/*
 * @brief Safely get the string content of a JSON node
 * @return Returns the corresponding C string pointer if it's JSON_STRING, otherwise returns an empty string ""
 */
const char *json_get_string(json_node_t *node)
{
    if (node && node->type == JSON_STRING && node->value.string) {
        return node->value.string;
    }
    return "";
}

/*
 * @brief Check if the JSON node is of type JSON_NULL
 */
int json_is_null(json_node_t *node)
{
    return node && node->type == JSON_NULL;
}

/*
 * @brief Check if the JSON node is of type JSON_OBJECT
 */
int json_is_object(json_node_t *node)
{
    return node && node->type == JSON_OBJECT;
}

/*
 * @brief Check if the JSON node is of type JSON_ARRAY
 */
int json_is_array(json_node_t *node)
{
    return node && node->type == JSON_ARRAY;
}

/*
 * @brief Get the child node of the specified key in a JSON object
 * 
 * Iterates through the key-value pair linked list under the object. Since each key-value pair uses a dummy node,
 * its actual structure is: Object Node -> Key Node -> Value Node -> Next Key Node...
 */
json_node_t *json_tree_get_child(json_node_t *parent, const char *key)
{
    // If the parent node is invalid, the key to query is null, or the parent node is not an object, return NULL directly
    if (!parent || !key || parent->type != JSON_OBJECT) {
        return NULL;
    }

    json_node_t *kv = parent->next; // Get the first key node
    while (kv) {
        // Compare if the key names match
        if (kv->key && strcmp(kv->key, key) == 0) {
            return kv->next; // Return the value node associated with the key
        }
        // Jump to the next key node, step is 2 (i.e., skip the current value node)
        kv = kv->next ? kv->next->next : NULL;
    }
    return NULL; // Corresponding key not found
}

/*
 * @brief Deeply destroy the entire JSON node tree and free memory
 */
void json_destroy(json_node_t *node)
{
    if (!node) {
        return;
    }

    // For composite nodes like objects and arrays, free their child nodes
    if (node->type == JSON_OBJECT || node->type == JSON_ARRAY) {
        json_node_t *child = node->next;
        while (child) {
            json_node_t *value   = child->next;
            json_node_t *next_kv = value ? value->next : NULL;

            // If the value node is a string type, the string memory needs to be freed separately
            if (value && value->type == JSON_STRING && value->value.string) {
                free(value->value.string);
                value->value.string = NULL;
            }
            free(value);
            value = NULL;

            // Free the key string of the key node
            if (child->key) {
                free(child->key);
                child->key = NULL;
            }
            free(child);
            // Advance to the next key-value pair or element
            child = next_kv;
        }
        node->next = NULL;
    }

    // Handle the case where the current node itself is a string or has a key
    if (node->type == JSON_STRING && node->value.string) {
        free(node->value.string);
        node->value.string = NULL;
    }
    if (node->key) {
        free(node->key);
        node->key = NULL;
    }

    // Free the node structure itself
    free(node);
}
