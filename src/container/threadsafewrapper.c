/**
 * @file threadsafewrapper.c
 * @brief Thread-safe container wrappers
 * @details Wraps core containers with mutex locking for concurrent access.
 */

#include "cobalt/container/threadsafewrapper.h"
#include "cobalt/container/deque.h"
#include "cobalt/container/queue.h"
#include "cobalt/container/set.h"
#include "cobalt/container/stack.h"
#include "cobalt/container/treemap.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>
#include <string.h>

/* ======================================================================== */
/* Thread-Safe Vector                                                        */
/* ========================================================================= */

/**
 * @brief Opaque thread-safe vector wrapper
 * @details Serializes every operation through the embedded mutex.
 */
struct cobalt_tsvector {
    cobalt_mutex_t     *mutex;
    cobalt_vector_t    *vec;
    cobalt_allocator_t *alloc;
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
    ts->alloc = alloc;
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
    vec->alloc->free(vec->alloc, vec);
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
/* ========================================================================= */

/**
 * @brief Opaque thread-safe hashmap wrapper
 * @details Serializes every operation through the embedded mutex.
 */
struct cobalt_tshashmap {
    cobalt_mutex_t     *mutex;
    cobalt_hashmap_t   *map;
    cobalt_allocator_t *alloc;
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
    ts->alloc = alloc;
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
    map->alloc->free(map->alloc, map);
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
/* ========================================================================= */

/**
 * @brief Opaque thread-safe list wrapper
 * @details Serializes every operation through the embedded mutex.
 */
struct cobalt_tslist {
    cobalt_mutex_t     *mutex;
    cobalt_list_t      *list;
    cobalt_allocator_t *alloc;
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
    ts->alloc = alloc;
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
    list->alloc->free(list->alloc, list);
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

/* ======================================================================== */
/* Thread-Safe Deque                                                         */
/* ========================================================================= */

struct cobalt_tsdeque {
    cobalt_mutex_t     *mutex;
    cobalt_deque_t     *d;
    cobalt_allocator_t *alloc;
};

cobalt_tsdeque_t *cobalt_tsdeque_create(void)
{
    return cobalt_tsdeque_create_with_allocator(cobalt_allocator_get_system());
}

cobalt_tsdeque_t *cobalt_tsdeque_create_with_allocator(cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_mutex_t *mutex = cobalt_mutex_create();
    if (!mutex) {
        return NULL;
    }
    cobalt_tsdeque_t *ts = (cobalt_tsdeque_t *)alloc->alloc(alloc, sizeof(cobalt_tsdeque_t));
    if (!ts) {
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->d = cobalt_deque_create_with_allocator(alloc);
    if (!ts->d) {
        alloc->free(alloc, ts);
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->mutex = mutex;
    ts->alloc = alloc;
    return ts;
}

void cobalt_tsdeque_destroy(cobalt_tsdeque_t *d)
{
    if (!d) {
        return;
    }
    cobalt_deque_destroy(d->d);
    cobalt_mutex_destroy(d->mutex);
    d->alloc->free(d->alloc, d);
}

int cobalt_tsdeque_push_front(cobalt_tsdeque_t *d, void *item)
{
    if (!d) {
        return -1;
    }
    cobalt_mutex_lock(d->mutex);
    int ret = cobalt_deque_push_front(d->d, item);
    cobalt_mutex_unlock(d->mutex);
    return ret;
}

int cobalt_tsdeque_push_back(cobalt_tsdeque_t *d, void *item)
{
    if (!d) {
        return -1;
    }
    cobalt_mutex_lock(d->mutex);
    int ret = cobalt_deque_push_back(d->d, item);
    cobalt_mutex_unlock(d->mutex);
    return ret;
}

void *cobalt_tsdeque_pop_front(cobalt_tsdeque_t *d)
{
    if (!d) {
        return NULL;
    }
    cobalt_mutex_lock(d->mutex);
    void *item = cobalt_deque_pop_front(d->d);
    cobalt_mutex_unlock(d->mutex);
    return item;
}

void *cobalt_tsdeque_pop_back(cobalt_tsdeque_t *d)
{
    if (!d) {
        return NULL;
    }
    cobalt_mutex_lock(d->mutex);
    void *item = cobalt_deque_pop_back(d->d);
    cobalt_mutex_unlock(d->mutex);
    return item;
}

void *cobalt_tsdeque_peek_front(const cobalt_tsdeque_t *d)
{
    if (!d) {
        return NULL;
    }
    cobalt_mutex_lock(d->mutex);
    void *item = cobalt_deque_peek_front(d->d);
    cobalt_mutex_unlock(d->mutex);
    return item;
}

void *cobalt_tsdeque_peek_back(const cobalt_tsdeque_t *d)
{
    if (!d) {
        return NULL;
    }
    cobalt_mutex_lock(d->mutex);
    void *item = cobalt_deque_peek_back(d->d);
    cobalt_mutex_unlock(d->mutex);
    return item;
}

size_t cobalt_tsdeque_size(const cobalt_tsdeque_t *d)
{
    if (!d) {
        return 0;
    }
    cobalt_mutex_lock(d->mutex);
    size_t sz = cobalt_deque_size(d->d);
    cobalt_mutex_unlock(d->mutex);
    return sz;
}

int cobalt_tsdeque_is_empty(const cobalt_tsdeque_t *d)
{
    if (!d) {
        return 1;
    }
    cobalt_mutex_lock(d->mutex);
    int empty = cobalt_deque_is_empty(d->d);
    cobalt_mutex_unlock(d->mutex);
    return empty;
}

/* ======================================================================== */
/* Thread-Safe Queue                                                         */
/* ========================================================================= */

struct cobalt_tsqueue {
    cobalt_mutex_t     *mutex;
    cobalt_queue_t     *q;
    cobalt_allocator_t *alloc;
};

cobalt_tsqueue_t *cobalt_tsqueue_create(void)
{
    return cobalt_tsqueue_create_with_allocator(cobalt_allocator_get_system());
}

cobalt_tsqueue_t *cobalt_tsqueue_create_with_allocator(cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_mutex_t *mutex = cobalt_mutex_create();
    if (!mutex) {
        return NULL;
    }
    cobalt_tsqueue_t *ts = (cobalt_tsqueue_t *)alloc->alloc(alloc, sizeof(cobalt_tsqueue_t));
    if (!ts) {
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->q = cobalt_queue_create_with_allocator(alloc);
    if (!ts->q) {
        alloc->free(alloc, ts);
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->mutex = mutex;
    ts->alloc = alloc;
    return ts;
}

void cobalt_tsqueue_destroy(cobalt_tsqueue_t *q)
{
    if (!q) {
        return;
    }
    cobalt_queue_destroy(q->q);
    cobalt_mutex_destroy(q->mutex);
    q->alloc->free(q->alloc, q);
}

int cobalt_tsqueue_enqueue(cobalt_tsqueue_t *q, void *item)
{
    if (!q) {
        return -1;
    }
    cobalt_mutex_lock(q->mutex);
    int ret = cobalt_queue_enqueue(q->q, item);
    cobalt_mutex_unlock(q->mutex);
    return ret;
}

void *cobalt_tsqueue_dequeue(cobalt_tsqueue_t *q)
{
    if (!q) {
        return NULL;
    }
    cobalt_mutex_lock(q->mutex);
    void *item = cobalt_queue_dequeue(q->q);
    cobalt_mutex_unlock(q->mutex);
    return item;
}

void *cobalt_tsqueue_peek(const cobalt_tsqueue_t *q)
{
    if (!q) {
        return NULL;
    }
    cobalt_mutex_lock(q->mutex);
    void *item = cobalt_queue_peek(q->q);
    cobalt_mutex_unlock(q->mutex);
    return item;
}

size_t cobalt_tsqueue_size(const cobalt_tsqueue_t *q)
{
    if (!q) {
        return 0;
    }
    cobalt_mutex_lock(q->mutex);
    size_t sz = cobalt_queue_size(q->q);
    cobalt_mutex_unlock(q->mutex);
    return sz;
}

int cobalt_tsqueue_is_empty(const cobalt_tsqueue_t *q)
{
    if (!q) {
        return 1;
    }
    cobalt_mutex_lock(q->mutex);
    int empty = cobalt_queue_is_empty(q->q);
    cobalt_mutex_unlock(q->mutex);
    return empty;
}

/* ======================================================================== */
/* Thread-Safe Stack                                                         */
/* ========================================================================= */

struct cobalt_tsstack {
    cobalt_mutex_t     *mutex;
    cobalt_stack_t     *s;
    cobalt_allocator_t *alloc;
};

cobalt_tsstack_t *cobalt_tsstack_create(void)
{
    return cobalt_tsstack_create_with_allocator(cobalt_allocator_get_system());
}

cobalt_tsstack_t *cobalt_tsstack_create_with_allocator(cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_mutex_t *mutex = cobalt_mutex_create();
    if (!mutex) {
        return NULL;
    }
    cobalt_tsstack_t *ts = (cobalt_tsstack_t *)alloc->alloc(alloc, sizeof(cobalt_tsstack_t));
    if (!ts) {
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->s = cobalt_stack_create_with_allocator(alloc);
    if (!ts->s) {
        alloc->free(alloc, ts);
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->mutex = mutex;
    ts->alloc = alloc;
    return ts;
}

void cobalt_tsstack_destroy(cobalt_tsstack_t *s)
{
    if (!s) {
        return;
    }
    cobalt_stack_destroy(s->s);
    cobalt_mutex_destroy(s->mutex);
    s->alloc->free(s->alloc, s);
}

int cobalt_tsstack_push(cobalt_tsstack_t *s, void *item)
{
    if (!s) {
        return -1;
    }
    cobalt_mutex_lock(s->mutex);
    int ret = cobalt_stack_push(s->s, item);
    cobalt_mutex_unlock(s->mutex);
    return ret;
}

void *cobalt_tsstack_pop(cobalt_tsstack_t *s)
{
    if (!s) {
        return NULL;
    }
    cobalt_mutex_lock(s->mutex);
    void *item = cobalt_stack_pop(s->s);
    cobalt_mutex_unlock(s->mutex);
    return item;
}

void *cobalt_tsstack_peek(const cobalt_tsstack_t *s)
{
    if (!s) {
        return NULL;
    }
    cobalt_mutex_lock(s->mutex);
    void *item = cobalt_stack_peek(s->s);
    cobalt_mutex_unlock(s->mutex);
    return item;
}

size_t cobalt_tsstack_size(const cobalt_tsstack_t *s)
{
    if (!s) {
        return 0;
    }
    cobalt_mutex_lock(s->mutex);
    size_t sz = cobalt_stack_size(s->s);
    cobalt_mutex_unlock(s->mutex);
    return sz;
}

int cobalt_tsstack_is_empty(const cobalt_tsstack_t *s)
{
    if (!s) {
        return 1;
    }
    cobalt_mutex_lock(s->mutex);
    int empty = cobalt_stack_is_empty(s->s);
    cobalt_mutex_unlock(s->mutex);
    return empty;
}

/* ======================================================================== */
/* Thread-Safe TreeMap                                                       */
/* ========================================================================= */

struct cobalt_tstreemap {
    cobalt_mutex_t     *mutex;
    cobalt_treemap_t   *m;
    cobalt_allocator_t *alloc;
};

cobalt_tstreemap_t *cobalt_tstreemap_create(void)
{
    return cobalt_tstreemap_create_with_allocator(cobalt_allocator_get_system());
}

cobalt_tstreemap_t *cobalt_tstreemap_create_ext(cobalt_compare_func_t compare_func)
{
    (void)compare_func;
    return cobalt_tstreemap_create();
}

cobalt_tstreemap_t *cobalt_tstreemap_create_with_allocator(cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_mutex_t *mutex = cobalt_mutex_create();
    if (!mutex) {
        return NULL;
    }
    cobalt_tstreemap_t *ts = (cobalt_tstreemap_t *)alloc->alloc(alloc, sizeof(cobalt_tstreemap_t));
    if (!ts) {
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->m = cobalt_treemap_create_with_allocator(alloc);
    if (!ts->m) {
        alloc->free(alloc, ts);
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->mutex = mutex;
    ts->alloc = alloc;
    return ts;
}

void cobalt_tstreemap_destroy(cobalt_tstreemap_t *m)
{
    if (!m) {
        return;
    }
    cobalt_treemap_destroy(m->m);
    cobalt_mutex_destroy(m->mutex);
    m->alloc->free(m->alloc, m);
}

int cobalt_tstreemap_put(cobalt_tstreemap_t *m, const char *key, void *value)
{
    if (!m || !key) {
        return -1;
    }
    cobalt_mutex_lock(m->mutex);
    int ret = cobalt_treemap_put(m->m, key, value);
    cobalt_mutex_unlock(m->mutex);
    return ret;
}

void *cobalt_tstreemap_get(const cobalt_tstreemap_t *m, const char *key)
{
    if (!m || !key) {
        return NULL;
    }
    cobalt_mutex_lock(m->mutex);
    void *val = cobalt_treemap_get(m->m, key);
    cobalt_mutex_unlock(m->mutex);
    return val;
}

int cobalt_tstreemap_remove(cobalt_tstreemap_t *m, const char *key)
{
    if (!m || !key) {
        return -1;
    }
    cobalt_mutex_lock(m->mutex);
    int ret = cobalt_treemap_remove(m->m, key);
    cobalt_mutex_unlock(m->mutex);
    return ret;
}

const char *cobalt_tstreemap_min_key(const cobalt_tstreemap_t *m)
{
    if (!m) {
        return NULL;
    }
    cobalt_mutex_lock(m->mutex);
    const char *k = cobalt_treemap_min_key(m->m);
    cobalt_mutex_unlock(m->mutex);
    return k;
}

const char *cobalt_tstreemap_max_key(const cobalt_tstreemap_t *m)
{
    if (!m) {
        return NULL;
    }
    cobalt_mutex_lock(m->mutex);
    const char *k = cobalt_treemap_max_key(m->m);
    cobalt_mutex_unlock(m->mutex);
    return k;
}

size_t cobalt_tstreemap_size(const cobalt_tstreemap_t *m)
{
    if (!m) {
        return 0;
    }
    cobalt_mutex_lock(m->mutex);
    size_t sz = cobalt_treemap_size(m->m);
    cobalt_mutex_unlock(m->mutex);
    return sz;
}

/* ======================================================================== */
/* Thread-Safe Set                                                           */
/* ========================================================================= */

struct cobalt_tsset {
    cobalt_mutex_t     *mutex;
    cobalt_set_t       *s;
    cobalt_allocator_t *alloc;
};

cobalt_tsset_t *cobalt_tsset_create(size_t initial_capacity)
{
    return cobalt_tsset_create_with_allocator(initial_capacity, cobalt_allocator_get_system());
}

cobalt_tsset_t *cobalt_tsset_create_with_allocator(size_t              initial_capacity,
                                                   cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_mutex_t *mutex = cobalt_mutex_create();
    if (!mutex) {
        return NULL;
    }
    cobalt_tsset_t *ts = (cobalt_tsset_t *)alloc->alloc(alloc, sizeof(cobalt_tsset_t));
    if (!ts) {
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->s = cobalt_set_create_with_allocator(initial_capacity, alloc);
    if (!ts->s) {
        alloc->free(alloc, ts);
        cobalt_mutex_destroy(mutex);
        return NULL;
    }
    ts->mutex = mutex;
    ts->alloc = alloc;
    return ts;
}

void cobalt_tsset_destroy(cobalt_tsset_t *s)
{
    if (!s) {
        return;
    }
    cobalt_set_destroy(s->s);
    cobalt_mutex_destroy(s->mutex);
    s->alloc->free(s->alloc, s);
}

int cobalt_tsset_insert(cobalt_tsset_t *s, void *item)
{
    if (!s) {
        return -1;
    }
    cobalt_mutex_lock(s->mutex);
    int ret = cobalt_set_insert(s->s, item);
    cobalt_mutex_unlock(s->mutex);
    return ret;
}

int cobalt_tsset_insert_ext(cobalt_tsset_t *s, const void *item, size_t item_len)
{
    if (!s) {
        return -1;
    }
    cobalt_mutex_lock(s->mutex);
    int ret = cobalt_set_insert_ext(s->s, item, item_len);
    cobalt_mutex_unlock(s->mutex);
    return ret;
}

int cobalt_tsset_remove(cobalt_tsset_t *s, void *item)
{
    if (!s) {
        return -1;
    }
    cobalt_mutex_lock(s->mutex);
    int ret = cobalt_set_remove(s->s, item);
    cobalt_mutex_unlock(s->mutex);
    return ret;
}

int cobalt_tsset_remove_ext(cobalt_tsset_t *s, const void *item, size_t item_len)
{
    if (!s) {
        return -1;
    }
    cobalt_mutex_lock(s->mutex);
    int ret = cobalt_set_remove_ext(s->s, item, item_len);
    cobalt_mutex_unlock(s->mutex);
    return ret;
}

int cobalt_tsset_contains(cobalt_tsset_t *s, void *item)
{
    if (!s) {
        return 0;
    }
    cobalt_mutex_lock(s->mutex);
    int found = cobalt_set_contains(s->s, item);
    cobalt_mutex_unlock(s->mutex);
    return found;
}

int cobalt_tsset_contains_ext(cobalt_tsset_t *s, const void *item, size_t item_len)
{
    if (!s) {
        return 0;
    }
    cobalt_mutex_lock(s->mutex);
    int found = cobalt_set_contains_ext(s->s, item, item_len);
    cobalt_mutex_unlock(s->mutex);
    return found;
}

size_t cobalt_tsset_size(const cobalt_tsset_t *s)
{
    if (!s) {
        return 0;
    }
    cobalt_mutex_lock(s->mutex);
    size_t sz = cobalt_set_size(s->s);
    cobalt_mutex_unlock(s->mutex);
    return sz;
}

int cobalt_tsset_is_empty(const cobalt_tsset_t *s)
{
    if (!s) {
        return 1;
    }
    cobalt_mutex_lock(s->mutex);
    int empty = cobalt_set_is_empty(s->s);
    cobalt_mutex_unlock(s->mutex);
    return empty;
}
