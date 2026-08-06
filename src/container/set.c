/**
 * @file set.c
 * @brief Set container implementation
 * @details Set data structure implemented by wrapping cobalt_hashmap.
 */

#include "cobalt/container/set.h"
#include "cobalt/container/hashmap.h"
#include "cobalt/runtime/error.h"
#include "cobalt/utils/string.h"
#include <stdlib.h>

struct cobalt_set {
    cobalt_hashmap_t *map;
};

cobalt_set_t *cobalt_set_create(size_t initial_capacity)
{
    cobalt_set_t *set = malloc(sizeof(cobalt_set_t));
    if (!set) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    set->map = cobalt_hashmap_create(initial_capacity);
    if (!set->map) {
        free(set);
        return NULL;
    }
    return set;
}

cobalt_set_t *cobalt_set_create_ext(size_t initial_capacity,
                                     cobalt_set_hash_func_t hash_func,
                                     cobalt_set_equal_func_t equal_func)
{
    cobalt_set_t *set = malloc(sizeof(cobalt_set_t));
    if (!set) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    set->map = cobalt_hashmap_create_ext(initial_capacity,
                                         (cobalt_hash_func_t)hash_func,
                                         (cobalt_equal_func_t)equal_func);
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
    if (!set || !item) return -1;
    return cobalt_hashmap_put(set->map, (const char *)item, item) == 0 ? 0 : -1;
}

int cobalt_set_insert_ext(cobalt_set_t *set, const void *item, size_t item_len)
{
    if (!set || !item) return -1;
    return cobalt_hashmap_put_ext(set->map, item, item_len, (void *)item) == 0 ? 0 : -1;
}

int cobalt_set_remove(cobalt_set_t *set, void *item)
{
    if (!set || !item) return -1;
    return cobalt_hashmap_remove(set->map, (const char *)item) == 0 ? 0 : -1;
}

int cobalt_set_remove_ext(cobalt_set_t *set, const void *item, size_t item_len)
{
    if (!set || !item) return -1;
    return cobalt_hashmap_remove_ext(set->map, item, item_len) == 0 ? 0 : -1;
}

int cobalt_set_contains(cobalt_set_t *set, void *item)
{
    if (!set || !item) return 0;
    return cobalt_hashmap_get(set->map, (const char *)item) != NULL;
}

int cobalt_set_contains_ext(cobalt_set_t *set, const void *item, size_t item_len)
{
    if (!set || !item) return 0;
    return cobalt_hashmap_get_ext(set->map, item, item_len) != NULL;
}

size_t cobalt_set_size(const cobalt_set_t *set)
{
    return set ? cobalt_hashmap_size(set->map) : 0;
}

int cobalt_set_is_empty(const cobalt_set_t *set)
{
    return set ? cobalt_hashmap_size(set->map) == 0 : 1;
}
