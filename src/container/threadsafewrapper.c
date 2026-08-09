/**
 * @file threadsafewrapper.c
 * @brief Thread-safe container wrappers
 * @details Wraps core containers with mutex locking for concurrent access.
 */

#include "cobalt/container/threadsafewrapper.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>
#include <string.h>

/* ======================================================================== */
/* Thread-Safe Vector                                                        */
/* ======================================================================== */

struct cobalt_tsvector {
    cobalt_mutex_t  *mutex;
    cobalt_vector_t *vec;
};

cobalt_tsvector_t *cobalt_tsvector_create(size_t initial_capacity)
{
    return cobalt_tsvector_create_with_allocator(initial_capacity, cobalt_allocator_get_system());
}

cobalt_tsvector_t *cobalt_tsvector_create_with_allocator(size_t              initial_capacity,
                                                         cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_mutex_t *mutex = cobalt_mutex_create();
    if (!mutex) {
        return NULL;
    }
    cobalt_tsvector_t *ts = (cobalt_tsvector_t *)alloc->alloc(alloc, sizeof(cobalt_tsvector_t));
    if (!ts) {
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->vec = cobalt_vector_create_with_allocator(initial_capacity, alloc);
    if (!ts->vec) {
        alloc->free(alloc, ts);
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->mutex = mutex;
    return ts;
}

void cobalt_tsvector_destroy(cobalt_tsvector_t *vec)
{
    if (!vec) {
        return;
    }
    cobalt_vector_destroy(vec->vec);
    cobalt_mutex_destroy(vec->mutex);
    cobalt_allocator_t *alloc = cobalt_allocator_get_system();
    alloc->free(alloc, vec);
}

int cobalt_tsvector_push(cobalt_tsvector_t *vec, void *item)
{
    if (!vec) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    cobalt_mutex_lock(vec->mutex);
    int ret = cobalt_vector_push(vec->vec, item);
    cobalt_mutex_unlock(vec->mutex);
    return ret;
}

void *cobalt_tsvector_get(const cobalt_tsvector_t *vec, size_t index)
{
    if (!vec) {
        return NULL;
    }
    cobalt_mutex_lock(vec->mutex);
    void *item = cobalt_vector_get(vec->vec, index);
    cobalt_mutex_unlock(vec->mutex);
    return item;
}

int cobalt_tsvector_set(cobalt_tsvector_t *vec, size_t index, void *item)
{
    if (!vec) {
        return -1;
    }
    cobalt_mutex_lock(vec->mutex);
    int ret = cobalt_vector_set(vec->vec, index, item);
    cobalt_mutex_unlock(vec->mutex);
    return ret;
}

void cobalt_tsvector_remove_at(cobalt_tsvector_t *vec, size_t index)
{
    if (!vec) {
        return;
    }
    cobalt_mutex_lock(vec->mutex);
    cobalt_vector_remove_at(vec->vec, index);
    cobalt_mutex_unlock(vec->mutex);
}

size_t cobalt_tsvector_size(const cobalt_tsvector_t *vec)
{
    if (!vec) {
        return 0;
    }
    cobalt_mutex_lock(vec->mutex);
    size_t sz = cobalt_vector_size(vec->vec);
    cobalt_mutex_unlock(vec->mutex);
    return sz;
}

int cobalt_tsvector_is_empty(const cobalt_tsvector_t *vec)
{
    if (!vec) {
        return 1;
    }
    cobalt_mutex_lock(vec->mutex);
    int empty = cobalt_vector_is_empty(vec->vec);
    cobalt_mutex_unlock(vec->mutex);
    return empty;
}

size_t cobalt_tsvector_capacity(const cobalt_tsvector_t *vec)
{
    if (!vec) {
        return 0;
    }
    cobalt_mutex_lock(vec->mutex);
    size_t cap = cobalt_vector_capacity(vec->vec);
    cobalt_mutex_unlock(vec->mutex);
    return cap;
}

int cobalt_tsvector_reserve(cobalt_tsvector_t *vec, size_t n)
{
    if (!vec) {
        return -1;
    }
    cobalt_mutex_lock(vec->mutex);
    int ret = cobalt_vector_reserve(vec->vec, n);
    cobalt_mutex_unlock(vec->mutex);
    return ret;
}

int cobalt_tsvector_shrink_to_fit(cobalt_tsvector_t *vec)
{
    if (!vec) {
        return -1;
    }
    cobalt_mutex_lock(vec->mutex);
    int ret = cobalt_vector_shrink_to_fit(vec->vec);
    cobalt_mutex_unlock(vec->mutex);
    return ret;
}

/* ======================================================================== */
/* Thread-Safe HashMap                                                       */
/* ======================================================================== */

struct cobalt_tshashmap {
    cobalt_mutex_t   *mutex;
    cobalt_hashmap_t *map;
};

cobalt_tshashmap_t *cobalt_tshashmap_create(size_t initial_buckets)
{
    return cobalt_tshashmap_create_with_allocator(initial_buckets, cobalt_allocator_get_system());
}

cobalt_tshashmap_t *cobalt_tshashmap_create_with_allocator(size_t              initial_buckets,
                                                           cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_mutex_t *mutex = cobalt_mutex_create();
    if (!mutex) {
        return NULL;
    }
    cobalt_tshashmap_t *ts = (cobalt_tshashmap_t *)alloc->alloc(alloc, sizeof(cobalt_tshashmap_t));
    if (!ts) {
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->map = cobalt_hashmap_create_with_allocator(initial_buckets, alloc);
    if (!ts->map) {
        alloc->free(alloc, ts);
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->mutex = mutex;
    return ts;
}

void cobalt_tshashmap_destroy(cobalt_tshashmap_t *map)
{
    if (!map) {
        return;
    }
    cobalt_hashmap_destroy(map->map);
    cobalt_mutex_destroy(map->mutex);
    cobalt_allocator_t *alloc = cobalt_allocator_get_system();
    alloc->free(alloc, map);
}

int cobalt_tshashmap_put(cobalt_tshashmap_t *map, const char *key, void *value)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    cobalt_mutex_lock(map->mutex);
    int ret = cobalt_hashmap_put(map->map, key, value);
    cobalt_mutex_unlock(map->mutex);
    return ret;
}

void *cobalt_tshashmap_get(const cobalt_tshashmap_t *map, const char *key)
{
    if (!map || !key) {
        return NULL;
    }
    cobalt_mutex_lock(map->mutex);
    void *val = cobalt_hashmap_get(map->map, key);
    cobalt_mutex_unlock(map->mutex);
    return val;
}

int cobalt_tshashmap_remove(cobalt_tshashmap_t *map, const char *key)
{
    if (!map || !key) {
        return -1;
    }
    cobalt_mutex_lock(map->mutex);
    int ret = cobalt_hashmap_remove(map->map, key);
    cobalt_mutex_unlock(map->mutex);
    return ret;
}

int cobalt_tshashmap_contains(const cobalt_tshashmap_t *map, const char *key)
{
    if (!map || !key) {
        return 0;
    }
    cobalt_mutex_lock(map->mutex);
    int found = cobalt_hashmap_get(map->map, key) != NULL;
    cobalt_mutex_unlock(map->mutex);
    return found;
}

size_t cobalt_tshashmap_size(const cobalt_tshashmap_t *map)
{
    if (!map) {
        return 0;
    }
    cobalt_mutex_lock(map->mutex);
    size_t sz = cobalt_hashmap_size(map->map);
    cobalt_mutex_unlock(map->mutex);
    return sz;
}

size_t cobalt_tshashmap_capacity(const cobalt_tshashmap_t *map)
{
    if (!map) {
        return 0;
    }
    cobalt_mutex_lock(map->mutex);
    size_t cap = cobalt_hashmap_capacity(map->map);
    cobalt_mutex_unlock(map->mutex);
    return cap;
}

/* ======================================================================== */
/* Thread-Safe List                                                          */
/* ======================================================================== */

struct cobalt_tslist {
    cobalt_mutex_t *mutex;
    cobalt_list_t  *list;
};

cobalt_tslist_t *cobalt_tslist_create(void)
{
    return cobalt_tslist_create_with_allocator(cobalt_allocator_get_system());
}

cobalt_tslist_t *cobalt_tslist_create_with_allocator(cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_mutex_t *mutex = cobalt_mutex_create();
    if (!mutex) {
        return NULL;
    }
    cobalt_tslist_t *ts = (cobalt_tslist_t *)alloc->alloc(alloc, sizeof(cobalt_tslist_t));
    if (!ts) {
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->list = cobalt_list_create_with_allocator(alloc);
    if (!ts->list) {
        alloc->free(alloc, ts);
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->mutex = mutex;
    return ts;
}

void cobalt_tslist_destroy(cobalt_tslist_t *list)
{
    if (!list) {
        return;
    }
    cobalt_list_destroy(list->list);
    cobalt_mutex_destroy(list->mutex);
    cobalt_allocator_t *alloc = cobalt_allocator_get_system();
    alloc->free(alloc, list);
}

int cobalt_tslist_push_front(cobalt_tslist_t *list, void *item)
{
    if (!list) {
        return -1;
    }
    cobalt_mutex_lock(list->mutex);
    int ret = cobalt_list_push_front(list->list, item);
    cobalt_mutex_unlock(list->mutex);
    return ret;
}

int cobalt_tslist_push_back(cobalt_tslist_t *list, void *item)
{
    if (!list) {
        return -1;
    }
    cobalt_mutex_lock(list->mutex);
    int ret = cobalt_list_push_back(list->list, item);
    cobalt_mutex_unlock(list->mutex);
    return ret;
}

void *cobalt_tslist_pop_front(cobalt_tslist_t *list)
{
    if (!list) {
        return NULL;
    }
    cobalt_mutex_lock(list->mutex);
    void *item = cobalt_list_pop_front(list->list);
    cobalt_mutex_unlock(list->mutex);
    return item;
}

void *cobalt_tslist_pop_back(cobalt_tslist_t *list)
{
    if (!list) {
        return NULL;
    }
    cobalt_mutex_lock(list->mutex);
    void *item = cobalt_list_pop_back(list->list);
    cobalt_mutex_unlock(list->mutex);
    return item;
}

void *cobalt_tslist_get(const cobalt_tslist_t *list, size_t index)
{
    if (!list) {
        return NULL;
    }
    cobalt_mutex_lock(list->mutex);
    void *item = cobalt_list_get(list->list, index);
    cobalt_mutex_unlock(list->mutex);
    return item;
}

int cobalt_tslist_remove_at(cobalt_tslist_t *list, size_t index)
{
    if (!list) {
        return -1;
    }
    cobalt_mutex_lock(list->mutex);
    int ret = cobalt_list_remove_at(list->list, index);
    cobalt_mutex_unlock(list->mutex);
    return ret;
}

size_t cobalt_tslist_size(const cobalt_tslist_t *list)
{
    if (!list) {
        return 0;
    }
    cobalt_mutex_lock(list->mutex);
    size_t sz = cobalt_list_size(list->list);
    cobalt_mutex_unlock(list->mutex);
    return sz;
}

int cobalt_tslist_is_empty(const cobalt_tslist_t *list)
{
    if (!list) {
        return 1;
    }
    cobalt_mutex_lock(list->mutex);
    int empty = cobalt_list_is_empty(list->list);
    cobalt_mutex_unlock(list->mutex);
    return empty;
}
