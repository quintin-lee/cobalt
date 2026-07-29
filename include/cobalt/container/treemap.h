#ifndef TREEMAP_H
#define TREEMAP_H

/**
 * @file treemap.h
 * @brief Red-black tree based sorted map
 */

#include "map.h"

typedef enum { NODE_RED, NODE_BLACK } node_color_t;

typedef struct cobalt_tree_node cobalt_tree_node_t;

struct cobalt_tree_node {
  const char *key;
  void *value;
  node_color_t color;
  cobalt_tree_node_t *left;
  cobalt_tree_node_t *right;
  cobalt_tree_node_t *parent;
};

typedef struct {
  cobalt_map_t base;
  cobalt_tree_node_t *root;
  size_t size;
} cobalt_treemap_t;

/* Create a new treemap */
cobalt_treemap_t *cobalt_treemap_create(void);

/* Destroy the treemap */
void cobalt_treemap_destroy(cobalt_treemap_t *map);

/* Set value for key */
int cobalt_treemap_put(cobalt_treemap_t *map, const char *key, void *value);

/* Get value by key */
void *cobalt_treemap_get(cobalt_treemap_t *map, const char *key);

/* Remove key from map */
int cobalt_treemap_remove(cobalt_treemap_t *map, const char *key);

/* Get minimum/maximum keys */
const char *cobalt_treemap_min_key(cobalt_treemap_t *map);
const char *cobalt_treemap_max_key(cobalt_treemap_t *map);

/* Map size */
size_t cobalt_treemap_size(cobalt_treemap_t *map);

#endif /* TREEMAP_H */
