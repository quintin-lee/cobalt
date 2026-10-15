#ifndef COBALT_JSON_INTERNAL_H
#define COBALT_JSON_INTERNAL_H

/**
 * @file json_internal.h
 * @brief Internal data structures and definitions for the JSON module
 */

#include "cobalt/memory/arena.h"
#include "cobalt/module/json.h"
#include <stdbool.h>

/**
 * @brief Internal JSON node structure
 *
 * @details Forms the building block of the JSON document tree. For composite types
 *          (objects and arrays), child nodes are linked via the `next` pointer:
 *          - Objects: Object_Node -> Key_Node -> Value_Node -> Key_Node -> ...
 *          - Arrays:  Array_Node -> Element_Node -> Element_Node -> ...
 *
 *          The `key` field is valid for object key nodes and string scalar nodes.
 *          The `value` union holds the actual data for leaf nodes.
 *          `arena` and `owns_arena` track memory lifecycle when backed by a chunked arena.
 */
struct json_node {
    json_type_t       type;       /**< Node type: scalar, array, or object */
    json_value_t      value;      /**< Union holding scalar data */
    struct json_node *next;       /**< Next sibling node in parent's child list */
    struct json_node *child;      /**< First child node for composite types (object/array) */
    char             *key;        /**< Key string for object members */
    cobalt_arena_t   *arena;      /**< Associated Arena (valid only on root node) */
    bool              owns_arena; /**< Whether root node owns and should destroy the arena */
};

#endif /* COBALT_JSON_INTERNAL_H */
