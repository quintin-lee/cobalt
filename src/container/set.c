/**
 * @file set.c
 * @brief Set container implementation
 * @details Set data structure implemented by wrapping cobalt_hashmap.
 *          Embeds cobalt_map_t as first member for polymorphic map interface.
 */

#include "cobalt/container/set.h"
#include "cobalt/container/hashmap.h"
#include "cobalt/interface/map.h"
#include "cobalt/runtime/error.h"
#include "cobalt/utils/string.h"
#include <stdlib.h>

/* Sentinel value used as map value to indicate set membership. */
static const int set_sentinel = 1;

/* -------------------------------------------------------------------------- */
/* Opaque type definition (must be before any function that dereferences it)  */
/* -------------------------------------------------------------------------- */

struct cobalt_set {
    cobalt_map_t       base;
    cobalt_hashmap_t  *map;
};

/* -------------------------------------------------------------------------- */
/* Map vtable (set-specific)                                                */
/* -------------------------------------------------------------------------- */

static void *set_map_get(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    if (cobalt_hashmap_get_ext(set->map, key, key_len) != NULL) {
        return (void *)&set_sentinel;
    }
    return NULL;
}

static int set_map_put(cobalt_map_t *self, const void *key, size_t key_len, void *value)
{
    (void)value;
    cobalt_set_t *set = (cobalt_set_t *)self;
    int ret = cobalt_hashmap_put_ext(set->map, key, key_len, (void *)&set_sentinel);
    return (ret == 0) ? 0 : -1;
}

static int set_map_remove(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    return cobalt_hashmap_remove_ext(set->map, key, key_len);
}

static int set_map_contains(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    return cobalt_hashmap_get_ext(set->map, key, key_len) != NULL;
}

static void set_map_clear(cobalt_map_t *self)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    cobalt_hashmap_destroy(set->map);
    set->map = cobalt_hashmap_create(0);
    if (!set->map) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
    }
}

static size_t set_map_size(cobalt_map_t *self)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    return cobalt_hashmap_size(set->map);
}

static int set_map_is_empty(cobalt_map_t *self)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    return cobalt_hashmap_size(set->map) == 0;
}

static cobalt_map_iterator_t *set_map_iterator(cobalt_map_t *self)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    return cobalt_hashmap_iterator_create(set->map);
}

static void set_map_destroy(cobalt_map_t *self)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    cobalt_set_destroy(set);
}

static const cobalt_map_t set_map_vtable = {
    .get      = set_map_get,
    .put      = set_map_put,
    .remove   = set_map_remove,
    .contains = set_map_contains,
    .clear    = set_map_clear,
    .size     = set_map_size,
    .is_empty = set_map_is_empty,
    .iterator = set_map_iterator,
    .destroy  = set_map_destroy,
};

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

cobalt_set_t *cobalt_set_create(size_t initial_capacity)
{
    cobalt_set_t *set = malloc(sizeof(cobalt_set_t));
    if (!set) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    set->base  = set_map_vtable;
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
    set->base  = set_map_vtable;
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

/* -------------------------------------------------------------------------- */
/* Public iterator factory                                                    */
/* -------------------------------------------------------------------------- */

cobalt_map_iterator_t *cobalt_set_iterator_create(cobalt_set_t *set)
{
    if (!set) {
        return NULL;
    }
    return cobalt_hashmap_iterator_create(set->map);
}
