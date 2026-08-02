#ifndef HASHMAP_H
#define HASHMAP_H

/**
 * @file hashmap.h
 * @brief Hash map (dictionary) container
 */

#include <stddef.h>

typedef struct cobalt_hashmap_node cobalt_hashmap_node_t;

struct cobalt_hashmap_node {
    const char            *key;
    void                  *value;
    cobalt_hashmap_node_t *next; /* For collision chaining */
};

/*
   Simplified implementation: no Map interface layer for now.
   The actual impl is hidden in .c file.
*/
typedef struct cobalt_hashmap cobalt_hashmap_t;

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

/* Bucket count (for debugging / resize verification) */
size_t cobalt_hashmap_capacity(const cobalt_hashmap_t *map);

#endif /* HASHMAP_H */
