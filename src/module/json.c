#include "module/json.h"
#include <stdlib.h>
#include <string.h>

json_node_t *json_parse(const char *text) {
  (void)text;
  /* Simplified JSON parser */
  return NULL;
}

char *json_serialize(json_node_t *node) {
  (void)node;
  return strdup("{}");
}

void json_destroy(json_node_t *node) {
  (void)node;
}

double json_get_number(json_node_t *node) {
  (void)node;
  return 0.0;
}

const char *json_get_string(json_node_t *node) {
  (void)node;
  return "";
}

int json_is_null(json_node_t *node) {
  (void)node;
  return 0;
}

int json_is_object(json_node_t *node) {
  (void)node;
  return 0;
}

int json_is_array(json_node_t *node) {
  (void)node;
  return 0;
}
