#include "cobalt/container/lfhashmap.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

cobalt_lfhashmap_t *cobalt_lfhashmap_create(cobalt_lfhash_func_t  hash_fn,
                                            cobalt_lfequal_func_t equal_fn)
{
    (void)hash_fn;
    (void)equal_fn;
    return NULL;
}

cobalt_lfhashmap_t *cobalt_lfhashmap_create_ptr(void)
{
    return NULL;
}

cobalt_lfhashmap_t *cobalt_lfhashmap_create_with_allocator(cobalt_lfhash_func_t  hash_fn,
                                                           cobalt_lfequal_func_t equal_fn,
                                                           cobalt_allocator_t   *alloc,
                                                           cobalt_ebr_t         *ebr,
                                                           bool                  fast_ptr_mode)
{
    (void)hash_fn;
    (void)equal_fn;
    (void)alloc;
    (void)ebr;
    (void)fast_ptr_mode;
    return NULL;
}

void cobalt_lfhashmap_destroy(cobalt_lfhashmap_t *map)
{
    (void)map;
}

int cobalt_lfhashmap_insert(cobalt_lfhashmap_t *map, void *key, void *value)
{
    (void)map;
    (void)key;
    (void)value;
    return COBALT_ERROR_GENERAL;
}

int cobalt_lfhashmap_put(cobalt_lfhashmap_t *map, void *key, void *value, void **out_old_value)
{
    (void)map;
    (void)key;
    (void)value;
    (void)out_old_value;
    return COBALT_ERROR_GENERAL;
}

int cobalt_lfhashmap_get(cobalt_lfhashmap_t *map, const void *key, void **out_value)
{
    (void)map;
    (void)key;
    (void)out_value;
    return COBALT_ERROR_NOT_FOUND;
}

bool cobalt_lfhashmap_contains(cobalt_lfhashmap_t *map, const void *key)
{
    (void)map;
    (void)key;
    return false;
}

int cobalt_lfhashmap_remove(cobalt_lfhashmap_t *map, const void *key, void **out_old_value)
{
    (void)map;
    (void)key;
    (void)out_old_value;
    return COBALT_ERROR_NOT_FOUND;
}

size_t cobalt_lfhashmap_size(const cobalt_lfhashmap_t *map)
{
    (void)map;
    return 0;
}

size_t cobalt_lfhashmap_bucket_count(const cobalt_lfhashmap_t *map)
{
    (void)map;
    return 0;
}

bool cobalt_lfhashmap_is_empty(const cobalt_lfhashmap_t *map)
{
    (void)map;
    return true;
}

cobalt_lfhashmap_guard_t cobalt_lfhashmap_pin(cobalt_lfhashmap_t *map)
{
    (void)map;
    cobalt_lfhashmap_guard_t g = {0};
    return g;
}

void cobalt_lfhashmap_unpin(cobalt_lfhashmap_guard_t *guard)
{
    (void)guard;
}

cobalt_lfhashmap_iter_t *cobalt_lfhashmap_iter_create(cobalt_lfhashmap_t *map)
{
    (void)map;
    return NULL;
}

bool cobalt_lfhashmap_iter_next(cobalt_lfhashmap_iter_t *iter, void **out_key, void **out_value)
{
    (void)iter;
    (void)out_key;
    (void)out_value;
    return false;
}

void cobalt_lfhashmap_iter_destroy(cobalt_lfhashmap_iter_t *iter)
{
    (void)iter;
}
