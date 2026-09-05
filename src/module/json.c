/**
 * @file json.c
 * @brief Implementation of JSON query and memory management interfaces
 *
 * @details Provides a lightweight, in-memory representation of parsed JSON documents.
 *          The internal `json_node` structure forms a linked-list tree where:
 *          - JSON objects are represented as alternating key/value nodes
 *          - JSON arrays are represented as a chain of value nodes
 *          - Scalars (string, number, boolean, null) are leaf nodes
 *
 *          The tree is intentionally simple: no separate dictionary or vector types
 *          are exposed. Traversal is performed by walking the `next` pointers with
 *          stride-2 skipping for object key-value pairs.
 */

#include "cobalt/module/json.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Internal JSON node structure
 *
 * @details Forms the building block of the JSON document tree. For composite types
 *          (objects and arrays), child nodes are linked via the `next` pointer:
 *          - Objects: Object_Node -> Key_Node -> Value_Node -> Key_Node -> ...
 *          - Arrays:  Array_Node -> Element_Node -> Element_Node -> ...
 *
 *          The `key` field is only valid for object key nodes and string scalar nodes.
 *          The `value` union holds the actual data for leaf nodes.
 */
struct json_node {
    json_type_t       type;  /**< Node type: scalar, array, or object */
    json_value_t      value; /**< Union holding scalar data (number, string, bool) */
    struct json_node *next;  /**< Next sibling node in the parent's child list */
    char *key; /**< Key string for object members; NULL for array elements and scalars */
};

/* ========================================================================= */
/* Type-predicate accessors                                                  */
/* ========================================================================= */

/**
 * @brief Safely retrieve the numeric value of a JSON node
 *
 * @details Returns the `number` field if the node is a JSON_NUMBER; otherwise
 *          returns 0.0. This provides a safe default rather than requiring the
 *          caller to check the type first.
 *
 * @param node Pointer to the JSON node (may be NULL)
 * @return The numeric value, or 0.0 if the node is NULL or not a number
 */
double json_get_number(json_node_t *node)
{
    if (node && node->type == JSON_NUMBER) {
        return node->value.number;
    }
    return 0.0;
}

/**
 * @brief Safely retrieve the string value of a JSON node
 *
 * @details Returns the `string` field if the node is a JSON_STRING and the string
 *          pointer is non-NULL; otherwise returns an empty string "". The returned
 *          pointer points to internal storage — do not free it.
 *
 * @param node Pointer to the JSON node (may be NULL)
 * @return The string value, or "" if the node is NULL or not a string
 */
const char *json_get_string(json_node_t *node)
{
    if (node && node->type == JSON_STRING && node->value.string) {
        return node->value.string;
    }
    return "";
}

/**
 * @brief Check whether a JSON node is of type JSON_NULL
 *
 * @param node Pointer to the JSON node (may be NULL)
 * @return Non-zero if the node is JSON_NULL, 0 otherwise
 */
int json_is_null(json_node_t *node)
{
    return node && node->type == JSON_NULL;
}

/**
 * @brief Check whether a JSON node is of type JSON_OBJECT
 *
 * @param node Pointer to the JSON node (may be NULL)
 * @return Non-zero if the node is JSON_OBJECT, 0 otherwise
 */
int json_is_object(json_node_t *node)
{
    return node && node->type == JSON_OBJECT;
}

/**
 * @brief Check whether a JSON node is of type JSON_ARRAY
 *
 * @param node Pointer to the JSON node (may be NULL)
 * @return Non-zero if the node is JSON_ARRAY, 0 otherwise
 */
int json_is_array(json_node_t *node)
{
    return node && node->type == JSON_ARRAY;
}

/* ========================================================================= */
/* Tree traversal                                                            */
/* ========================================================================= */

/**
 * @brief Retrieve a child node by key from a JSON object
 *
 * @details Walks the linked list of key-value pairs under `parent`. Because objects
 *          store children as alternating key/value nodes, the traversal advances by
 *          two `next` pointers per iteration (skipping from key to next key).
 *
 *          The node layout for an object is:
 * @code
 *          Object_Node -> Key_Node("foo") -> Value_Node(...) -> Key_Node("bar") -> Value_Node(...)
 * @endcode
 *
 * @param parent Pointer to the JSON object node
 * @param key    Null-terminated key string to search for
 * @return Pointer to the value node associated with `key`, or NULL if not found
 */
json_node_t *json_tree_get_child(json_node_t *parent, const char *key)
{
    if (!parent || !key || parent->type != JSON_OBJECT) {
        return NULL;
    }

    json_node_t *kv = parent->next; /* First key node */
    while (kv) {
        if (kv->key && strcmp(kv->key, key) == 0) {
            return kv->next; /* Return the value node paired with this key */
        }
        kv = kv->next ? kv->next->next : NULL;
    }
    return NULL;
}

/* ========================================================================= */
/* Memory management                                                         */
/* ========================================================================= */

/**
 * @brief Recursively destroy a JSON node tree and free all associated memory
 *
 * @details Walks the entire subtree rooted at `node`, freeing:
 *          - For objects/arrays: all child key-value pairs or elements, including
 *            their string data and key strings
 *          - For string scalars: the heap-allocated string buffer
 *          - The node structure itself
 *
 *          This function is safe to call with NULL. After destruction, all pointers
 *          into the freed tree become dangling — callers should discard them.
 */
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

            if (child && child->type == JSON_STRING && child->value.string) {
                free(child->value.string);
                child->value.string = NULL;
            }

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
