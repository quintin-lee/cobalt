/**
 * @file map.c
 * @brief Map interface implementation
 */
#include "cobalt/interface/map.h"
#include <stdlib.h>

/* -------------------------------------------------------------------------- */
/* Map iterator public API                                                    */
/* -------------------------------------------------------------------------- */

cobalt_map_iterator_t *cobalt_map_iterator_create(cobalt_map_t *map)
{
    if (!map || !map->iterator) {
        return NULL;
    }
    return map->iterator(map);
}

int cobalt_map_iterator_has_next(cobalt_map_iterator_t *iter)
{
    if (!iter || !iter->vtable) {
        return 0;
    }
    return iter->vtable->has_next(iter->data);
}

cobalt_map_pair_t cobalt_map_iterator_next(cobalt_map_iterator_t *iter)
{
    cobalt_map_pair_t empty = {NULL, NULL};
    if (!iter || !iter->vtable) {
        return empty;
    }
    return iter->vtable->next(iter->data);
}

void cobalt_map_iterator_destroy(cobalt_map_iterator_t *iter)
{
    if (!iter) {
        return;
    }
    if (iter->vtable && iter->vtable->destroy) {
        iter->vtable->destroy(iter->data);
    }
    free(iter);
}

/* -------------------------------------------------------------------------- */
/* Convenience API                                                            */
/* -------------------------------------------------------------------------- */

void *cobalt_map_get(cobalt_map_t *map, const void *key, size_t key_len)
{
    if (!map || !map->get) {
        return NULL;
    }
    return map->get(map, key, key_len);
}

int cobalt_map_put(cobalt_map_t *map, const void *key, size_t key_len, void *value)
{
    if (!map || !map->put) {
        return -1;
    }
    return map->put(map, key, key_len, value);
}

int cobalt_map_remove(cobalt_map_t *map, const void *key, size_t key_len)
{
    if (!map || !map->remove) {
        return -1;
    }
    return map->remove(map, key, key_len);
}

int cobalt_map_contains(cobalt_map_t *map, const void *key, size_t key_len)
{
    if (!map || !map->contains) {
        return 0;
    }
    return map->contains(map, key, key_len);
}

void cobalt_map_clear(cobalt_map_t *map)
{
    if (!map || !map->clear) {
        return;
    }
    map->clear(map);
}

size_t cobalt_map_size(cobalt_map_t *map)
{
    if (!map || !map->size) {
        return 0;
    }
    return map->size(map);
}

int cobalt_map_is_empty(cobalt_map_t *map)
{
    if (!map || !map->is_empty) {
        return 1;
    }
    return map->is_empty(map);
}
