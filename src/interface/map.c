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
