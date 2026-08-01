#ifndef TREEMAP_H
#define TREEMAP_H

/**
 * @file treemap.h
 * @brief Red-black tree based sorted map (simplified implementation)
 */

#include <stddef.h>

/* Opaque treemap type - implementation hidden in .c file, represented as void* */
typedef struct cobalt_treemap cobalt_treemap_t;

/* Create a new treemap */
cobalt_treemap_t* cobalt_treemap_create(void);

/* Destroy the treemap */
void cobalt_treemap_destroy(cobalt_treemap_t* map);

/* Set value for key */
int cobalt_treemap_put(cobalt_treemap_t* map, const char* key, void* value);

/* Get value by key */
void* cobalt_treemap_get(cobalt_treemap_t* map, const char* key);

/* Remove key from map */
int cobalt_treemap_remove(cobalt_treemap_t* map, const char* key);

/* Get minimum/maximum keys */
const char* cobalt_treemap_min_key(cobalt_treemap_t* map);
const char* cobalt_treemap_max_key(cobalt_treemap_t* map);

/* Map size */
size_t cobalt_treemap_size(cobalt_treemap_t* map);

#endif /* TREEMAP_H */
