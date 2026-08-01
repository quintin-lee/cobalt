#ifndef MAP_H
#define MAP_H

/**
 * @file map.h
 * @brief Key-value mapping interface
 */

#include <stddef.h>

typedef struct cobalt_map cobalt_map_t;

/* Map interface */
struct cobalt_map
{
    void* (*get)(cobalt_map_t* self, const void* key);
    int (*put)(cobalt_map_t* self, const void* key, void* value);
    int (*remove)(cobalt_map_t* self, const void* key);
    size_t (*size)(cobalt_map_t* self);
    int (*is_empty)(cobalt_map_t* self);
};

/* Create a map */
cobalt_map_t* cobalt_map_create(void);

/* Destroy a map */
void cobalt_map_destroy(cobalt_map_t* map);

#endif /* MAP_H */
