#ifndef HASHMAP_H
#define HASHMAP_H

/**
 * @file hashmap.h
 * @brief Hash map (dictionary) container
 */

#include "map.h"

typedef struct cobalt_hashmap_node cobalt_hashmap_node_t;

struct cobalt_hashmap_node {
  const char *key;
  void *value;
  cobalt_hashmap_node_t *next; /* For collision chaining */
};

typedef struct {
  cobalt_map_t base;
  cobalt_hashmap_node_t **buckets;
  size_t bucket_count;
  size_t size;
} cobalt_hashmap_t;

/* Create a new hash map */
cobalt_hashmap_t *cobalt_hashmap_create(size_t initial_buckets);

/* Destroy the hash map */
void cobalt_hashmap_destroy(cobalt_hashmap_t *map);

/* Set value for key */
int cobalt_hashmap_put(cobalt_hashmap_t *map, const char *key, void *value);

/* Get value by key */
void *cobalt_hashmap_get(cobalt_hashmap_t *map, const char *key);

/* Remove key from map */
int cobalt_hashmap_remove(cobalt_hashmap_t *map, const char *key);

/* Map size */
size_t cobalt_hashmap_size(cobalt_hashmap_t *map);

#endif /* HASHMAP_H */
