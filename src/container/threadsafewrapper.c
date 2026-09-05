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

/**
 * @brief create thread-safe vector using system allocator
 * @param initial_capacity backing vector capacity hint
 * @return new wrapper, or NULL on allocation failure
 */
cobalt_tsvector_t *cobalt_tsvector_create(size_t initial_capacity)
{
    return cobalt_tsvector_create_with_allocator(initial_capacity, cobalt_allocator_get_system());
}

/**
 * @brief create thread-safe vector using given allocator
 * @param initial_capacity backing vector capacity hint
 * @param alloc allocator backing wrapper and vector
 * @return new wrapper, or NULL on allocation failure
 */
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

/**
 * @brief destroy wrapper, vector and mutex
 * @param vec wrapper to destroy, NULL is no-op
 */
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

/**
 * @brief append item under lock
 * @param vec wrapper instance
 * @param item element to append
 * @return 0 on success, -1 on failure
 */
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

/**
 * @brief fetch element by index under lock
 * @param vec wrapper instance
 * @param index position to fetch
 * @return element pointer, or NULL when index out of range
 */
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

/**
 * @brief replace element at index under lock
 * @param vec wrapper instance
 * @param index position to overwrite
 * @param item new element pointer
 * @return 0 on success, -1 on failure
 */
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

/**
 * @brief remove element at index under lock
 * @param vec wrapper instance
 * @param index position to remove
 */
void cobalt_tsvector_remove_at(cobalt_tsvector_t *vec, size_t index)
{
    if (!vec) {
        return;
    }
    cobalt_mutex_lock(vec->mutex);
    cobalt_vector_remove_at(vec->vec, index);
    cobalt_mutex_unlock(vec->mutex);
}

/**
 * @brief report element count under lock
 * @param vec wrapper instance
 * @return number of elements, zero for NULL wrapper
 */
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

/**
 * @brief report emptiness under lock
 * @param vec wrapper instance
 * @return nonzero when empty, zero otherwise
 */
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

/**
 * @brief report backing capacity under lock
 * @param vec wrapper instance
 * @return capacity, zero for NULL wrapper
 */
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

/**
 * @brief preallocate capacity under lock
 * @param vec wrapper instance
 * @param n minimum capacity to reserve
 * @return 0 on success, -1 on failure
 */
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

/**
 * @brief release spare capacity under lock
 * @param vec wrapper instance
 * @return 0 on success, -1 on failure
 */
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

/**
 * @brief create thread-safe hash map using system allocator
 * @param initial_buckets backing map bucket hint
 * @return new wrapper, or NULL on allocation failure
 */
cobalt_tshashmap_t *cobalt_tshashmap_create(size_t initial_buckets)
{
    return cobalt_tshashmap_create_with_allocator(initial_buckets, cobalt_allocator_get_system());
}

/**
 * @brief create thread-safe hash map using given allocator
 * @param initial_buckets backing map bucket hint
 * @param alloc allocator backing wrapper and map
 * @return new wrapper, or NULL on allocation failure
 */
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

/**
 * @brief destroy wrapper, map and mutex
 * @param map wrapper to destroy, NULL is no-op
 */
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

/**
 * @brief insert or replace value under lock
 * @param map wrapper instance
 * @param key string key to copy
 * @param value value pointer to store
 * @return 0 on success, -1 on failure
 */
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

/**
 * @brief look up value under lock
 * @param map wrapper instance
 * @param key string key to search
 * @return value pointer, or NULL when absent
 */
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

/**
 * @brief remove entry under lock
 * @param map wrapper instance
 * @param key string key to remove
 * @return 0 on success, -1 when key absent
 */
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

/**
 * @brief test key presence under lock
 * @param map wrapper instance
 * @param key string key to test
 * @return nonzero when key exists, zero otherwise
 */
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

/**
 * @brief report entry count under lock
 * @param map wrapper instance
 * @return number of entries, zero for NULL wrapper
 */
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

/**
 * @brief report backing bucket count under lock
 * @param map wrapper instance
 * @return bucket count, zero for NULL wrapper
 */
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

/**
 * @brief create thread-safe list using system allocator
 * @return new wrapper, or NULL on allocation failure
 */
cobalt_tslist_t *cobalt_tslist_create(void)
{
    return cobalt_tslist_create_with_allocator(cobalt_allocator_get_system());
}

/**
 * @brief create thread-safe list using given allocator
 * @param alloc allocator backing wrapper and list
 * @return new wrapper, or NULL on allocation failure
 */
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

/**
 * @brief destroy wrapper, list and mutex
 * @param list wrapper to destroy, NULL is no-op
 */
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

/**
 * @brief push item at front under lock
 * @param list wrapper instance
 * @param item element to prepend
 * @return 0 on success, -1 on failure
 */
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

/**
 * @brief push item at back under lock
 * @param list wrapper instance
 * @param item element to append
 * @return 0 on success, -1 on failure
 */
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

/**
 * @brief pop front item under lock
 * @param list wrapper instance
 * @return front element, or NULL when empty
 */
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

/**
 * @brief pop back item under lock
 * @param list wrapper instance
 * @return back element, or NULL when empty
 */
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

/**
 * @brief fetch element by index under lock
 * @param list wrapper instance
 * @param index position to fetch
 * @return element pointer, or NULL when index out of range
 */
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

/**
 * @brief remove element at index under lock
 * @param list wrapper instance
 * @param index position to remove
 * @return 0 on success, -1 on failure
 */
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

/**
 * @brief report element count under lock
 * @param list wrapper instance
 * @return number of elements, zero for NULL wrapper
 */
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

/**
 * @brief report emptiness under lock
 * @param list wrapper instance
 * @return nonzero when empty, zero otherwise
 */
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
