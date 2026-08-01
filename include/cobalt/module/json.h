#ifndef JSON_H
#define JSON_H

/**
 * @file json.h
 * @brief JSON parsing and serialization module
 */

#include <stddef.h>

/* JSON value types */
typedef enum {
  JSON_NULL = 0,
  JSON_TRUE,
  JSON_FALSE,
  JSON_NUMBER,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT,
} json_type_t;

/* JSON value union */
typedef union {
  double number;
  char *string;
} json_value_t;

/* JSON structure */
typedef struct json_node json_node_t;

struct json_node {
  json_type_t type;
  json_value_t value;
  struct json_node *next; /* For array/object chaining */
  char *key;              /* For object keys (null in arrays) */
};

/* JSON operations */
json_node_t *json_parse(const char *text);
char *json_serialize(json_node_t *node);
void json_destroy(json_node_t *node);

/* Child/lookup helpers */
json_node_t *json_tree_get_child(json_node_t *parent, const char *key);

/* Value accessors */
double json_get_number(json_node_t *node);
const char *json_get_string(json_node_t *node);
int json_is_null(json_node_t *node);
int json_is_object(json_node_t *node);
int json_is_array(json_node_t *node);

#endif /* JSON_H */
