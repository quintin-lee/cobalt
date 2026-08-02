#include "cobalt/container/set.h"
#include "cobalt/container/hashmap.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

struct cobalt_set {
    cobalt_hashmap_t *map;
};

cobalt_set_t *cobalt_set_create(size_t initial_capacity)
{
    cobalt_set_t *set = malloc(sizeof(cobalt_set_t));
    if (!set) {
        return NULL;
    }

    set->map = cobalt_hashmap_create(initial_capacity);
    if (!set->map) {
        free(set);
        return NULL;
    }

    return set;
}

void cobalt_set_destroy(cobalt_set_t *set)
{
    if (set) {
        cobalt_hashmap_destroy(set->map);
        free(set);
    }
}

int cobalt_set_insert(cobalt_set_t *set, void *item)
{
    if (!set) {
        return -1;
    }
    return cobalt_hashmap_put(set->map, (const char *)item, item) == 0 ? 0 : -1;
}

int cobalt_set_remove(cobalt_set_t *set, void *item)
{
    if (!set || !item) {
        return -1;
    }
    return cobalt_hashmap_remove(set->map, (const char *)item) == 0 ? 0 : -1;
}

int cobalt_set_contains(cobalt_set_t *set, void *item)
{
    if (!set || !item) {
        return 0;
    }
    return cobalt_hashmap_get(set->map, (const char *)item) != NULL;
}

size_t cobalt_set_size(cobalt_set_t *set)
{
    if (!set) {
        return 0;
    }
    return cobalt_hashmap_size(set->map);
}

int cobalt_set_is_empty(cobalt_set_t *set)
{
    return set ? cobalt_hashmap_size(set->map) == 0 : 1;
}
