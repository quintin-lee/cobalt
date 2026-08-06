#ifndef JSON_H
#define JSON_H

/**
 * @file json.h
 * @brief JSON Parsing and Serialization Module
 * @details Provides simple in-memory JSON tree structure construction, querying, parsing (from
 * string), and serialization (to string) capabilities.
 */

#include <stddef.h>

/**
 * @defgroup JSON_Module JSON Processing Module
 * @{
 */

/**
 * @brief JSON Value Type Enumeration
 */
typedef enum {
    JSON_NULL = 0, /**< Null type */
    JSON_TRUE,     /**< Boolean true type */
    JSON_FALSE,    /**< Boolean false type */
    JSON_NUMBER,   /**< Number type (floating point) */
    JSON_STRING,   /**< String type */
    JSON_ARRAY,    /**< Array type */
    JSON_OBJECT,   /**< Object type (key-value pair collection) */
} json_type_t;

/**
 * @brief JSON Value Union
 */
typedef union {
    double number; /**< Stores number type values */
    char  *string; /**< Stores string type values, requires dynamic memory allocation */
} json_value_t;

/**
 * @brief JSON tree node (opaque — use accessor functions)
 */
typedef struct json_node json_node_t;

/* -----------------------------------------------------------------------------
 *  Core JSON Operations
 * -------------------------------------------------------------------------- */

/**
 * @brief Parse a JSON string
 * @param text Null-terminated JSON formatted text
 * @return Returns the root node of the parsed JSON node tree on success, NULL on failure
 */
json_node_t *json_parse(const char *text);

/**
 * @brief Serialize a JSON node tree to a string
 * @param node Root node of the JSON node tree
 * @return Returns a dynamically allocated JSON string on success (caller must free manually), NULL
 * or string of empty object on failure
 */
char *json_serialize(json_node_t *node);

/**
 * @brief Destroy a JSON node tree and free its occupied memory
 * @param node JSON node to destroy
 */
void json_destroy(json_node_t *node);

/* -----------------------------------------------------------------------------
 *  JSON Query Helper Functions
 * -------------------------------------------------------------------------- */

/**
 * @brief Find a child node with the specified key in a JSON object
 * @param parent Must be a parent node of type JSON_OBJECT
 * @param key Key name to look for
 * @return Pointer to the value node corresponding to the matching key, or NULL if not found or
 * parent is not an object
 */
json_node_t *json_tree_get_child(json_node_t *parent, const char *key);

/* -----------------------------------------------------------------------------
 *  JSON Value Reading Helper Functions
 * -------------------------------------------------------------------------- */

/**
 * @brief Get the value of a number node
 * @param node JSON node
 * @return The numeric value of the node, or 0.0 if not a number type
 */
double json_get_number(json_node_t *node);

/**
 * @brief Get the value of a string node
 * @param node JSON node
 * @return Pointer to the string content of the node, or empty string "" if not a string type
 */
const char *json_get_string(json_node_t *node);

/**
 * @brief Check if the node is null
 * @param node JSON node
 * @return 1 if JSON_NULL type, 0 otherwise
 */
int json_is_null(json_node_t *node);

/**
 * @brief Check if the node is an object
 * @param node JSON node
 * @return 1 if JSON_OBJECT type, 0 otherwise
 */
int json_is_object(json_node_t *node);

/**
 * @brief Check if the node is an array
 * @param node JSON node
 * @return 1 if JSON_ARRAY type, 0 otherwise
 */
int json_is_array(json_node_t *node);

/** @} */

#endif /* JSON_H */
