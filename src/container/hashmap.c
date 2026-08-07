/**
 * @file hashmap.c
 * @brief Hash map (dictionary) container implementation
 * @details Implements a hash map based on string keys. Uses the FNV-1a algorithm for hash
 * calculation and separate chaining for hash collision resolution.
 */

#include "cobalt/container/hashmap.h"
#include "cobalt/interface/map.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/runtime/error.h"
#include "cobalt/utils/string.h"
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* FNV-1a hash for string keys                                               */
/* -------------------------------------------------------------------------- */

static unsigned int hash_string(const char *str)
{
    unsigned int h = 2166136261U;
    while (*str) {
        h = (h ^ (unsigned char)*str) * 16777619U;
        str++;
    }
    return h;
}

/* -------------------------------------------------------------------------- */
/* Internal node and impl structures                                         */
/* -------------------------------------------------------------------------- */

typedef struct hashmap_node {
    void                *key;
    void                *value;
    struct hashmap_node *next;
    size_t               key_len;
    int                  key_owned;
} hashmap_node_t;

typedef struct {
    hashmap_node_t    **buckets;
    size_t              bucket_count;
    size_t              size;
    cobalt_hash_func_t  hash_func;
    cobalt_equal_func_t equal_func;
    cobalt_allocator_t *alloc; /* For destroy only; internal alloc uses stdlib */
} hashmap_impl_t;

struct cobalt_hashmap {
    cobalt_map_t   base;
    hashmap_impl_t impl;
};

/* -------------------------------------------------------------------------- */
/* Map iterator (hashmap-specific)                                           */
/* -------------------------------------------------------------------------- */

typedef struct {
    hashmap_impl_t *impl;
    hashmap_node_t *node;
    size_t          bucket_idx;
} hashmap_map_iter_t;

static int hashmap_map_iter_has_next(void *ctx)
{
    hashmap_map_iter_t *iter = (hashmap_map_iter_t *)ctx;
    return iter->node != NULL;
}

static cobalt_map_pair_t hashmap_map_iter_next(void *ctx)
{
    hashmap_map_iter_t *iter = (hashmap_map_iter_t *)ctx;
    cobalt_map_pair_t   pair = {NULL, NULL};
    if (!iter->node) {
        return pair;
    }

    hashmap_node_t *current = iter->node;
    iter->node              = current->next;

    /* Advance to next bucket if current chain is exhausted */
    if (!iter->node) {
        iter->bucket_idx++;
        while (iter->bucket_idx < iter->impl->bucket_count) {
            iter->node = iter->impl->buckets[iter->bucket_idx];
            if (iter->node) {
                break;
            }
            iter->bucket_idx++;
        }
    }

    pair.key   = current->key;
    pair.value = current->value;
    return pair;
}

static void hashmap_map_iter_destroy(void *ctx)
{
    free(ctx);
}

static const cobalt_map_iterator_vtable_t hashmap_map_iter_vtable = {
    .has_next = hashmap_map_iter_has_next,
    .next     = hashmap_map_iter_next,
    .destroy  = hashmap_map_iter_destroy,
};

static cobalt_map_iterator_t *hashmap_map_iterator(cobalt_map_t *self)
{
    cobalt_hashmap_t *map  = (cobalt_hashmap_t *)self;
    hashmap_impl_t   *impl = &map->impl;

    hashmap_map_iter_t *iter_state = calloc(1, sizeof(hashmap_map_iter_t));
    if (!iter_state) {
        return NULL;
    }

    /* Find first non-empty bucket */
    size_t          bucket_idx = 0;
    hashmap_node_t *node       = NULL;
    while (bucket_idx < impl->bucket_count) {
        node = impl->buckets[bucket_idx];
        if (node) {
            break;
        }
        bucket_idx++;
    }

    iter_state->impl       = impl;
    iter_state->node       = node;
    iter_state->bucket_idx = bucket_idx;

    cobalt_map_iterator_t *iter = malloc(sizeof(cobalt_map_iterator_t));
    if (!iter) {
        free(iter_state);
        return NULL;
    }
    iter->vtable = &hashmap_map_iter_vtable;
    iter->data   = iter_state;
    return iter;
}

/* -------------------------------------------------------------------------- */
/* Map interface function implementations                                    */
/* -------------------------------------------------------------------------- */

static void *hashmap_map_get(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_get_ext(map, key, key_len);
}

static int hashmap_map_put(cobalt_map_t *self, const void *key, size_t key_len, void *value)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_put_ext(map, key, key_len, value);
}

static int hashmap_map_remove(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_remove_ext(map, key, key_len);
}

static size_t hashmap_map_size(cobalt_map_t *self)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_size(map);
}

static int hashmap_map_is_empty(cobalt_map_t *self)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_size(map) == 0;
}

static void hashmap_map_destroy(cobalt_map_t *self)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    cobalt_hashmap_destroy(map);
}

static int hashmap_map_contains(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_get_ext(map, key, key_len) != NULL;
}

static void hashmap_free_node(hashmap_impl_t *impl, hashmap_node_t *node)
{
    if (!impl || !node) {
        return;
    }
    if (node->key_owned) {
        impl->alloc->free(impl->alloc, node->key);
    }
    impl->alloc->free(impl->alloc, node);
}

static void hashmap_map_clear(cobalt_map_t *self)
{
    cobalt_hashmap_t *map  = (cobalt_hashmap_t *)self;
    hashmap_impl_t   *impl = &map->impl;
    if (!impl->buckets) {
        return;
    }
    for (size_t i = 0; i < impl->bucket_count; i++) {
        hashmap_node_t *node = impl->buckets[i];
        while (node) {
            hashmap_node_t *next = node->next;
            hashmap_free_node(impl, node);
            node = next;
        }
        impl->buckets[i] = NULL;
    }
    impl->size = 0;
}

static const cobalt_map_t hashmap_map_vtable = {
    .get      = hashmap_map_get,
    .put      = hashmap_map_put,
    .remove   = hashmap_map_remove,
    .contains = hashmap_map_contains,
    .clear    = hashmap_map_clear,
    .size     = hashmap_map_size,
    .is_empty = hashmap_map_is_empty,
    .iterator = hashmap_map_iterator,
    .destroy  = hashmap_map_destroy,
};

/* -------------------------------------------------------------------------- */
/* Public iterator factory                                                    */
/* -------------------------------------------------------------------------- */

/// @brief Create a map iterator for this hash map
/// @param map Hash map instance
/// @return Iterator pointer, or NULL on failure
/// @note Returns a cobalt_map_iterator_t compatible with the Map interface.
///       Destroy with cobalt_map_iterator_destroy().
cobalt_map_iterator_t *cobalt_hashmap_iterator_create(cobalt_hashmap_t *map)
{
    if (!map) {
        return NULL;
    }
    return hashmap_map_iterator((cobalt_map_t *)map);
}

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* -------------------------------------------------------------------------- */

static unsigned int node_hash(const hashmap_impl_t *impl, const void *key, size_t key_len)
{
    if (impl->hash_func) {
        return impl->hash_func(key, key_len);
    }
    return hash_string((const char *)key);
}

static int
node_equal(const hashmap_impl_t *impl, const hashmap_node_t *node, const void *key, size_t key_len)
{
    if (impl->equal_func) {
        return impl->equal_func(key, node->key, key_len) != 0;
    }
    return strcmp((const char *)key, (const char *)node->key) == 0;
}

static int hashmap_ensure_buckets(hashmap_impl_t *impl, size_t min_buckets)
{
    if (impl->bucket_count >= min_buckets) {
        return 0;
    }
    size_t           new_count = min_buckets > 0 ? min_buckets : 16;
    hashmap_node_t **new_buckets =
        (hashmap_node_t **)impl->alloc->alloc(impl->alloc, new_count * sizeof(hashmap_node_t *));
    if (!new_buckets) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    memset(new_buckets, 0, new_count * sizeof(hashmap_node_t *));
    if (impl->buckets) {
        for (size_t i = 0; i < impl->bucket_count; i++) {
            hashmap_node_t *node = impl->buckets[i];
            while (node) {
                hashmap_node_t *next    = node->next;
                unsigned int    new_idx = node_hash(impl, node->key, node->key_len) % new_count;
                node->next              = new_buckets[new_idx];
                new_buckets[new_idx]    = node;
                node                    = next;
            }
        }
        impl->alloc->free(impl->alloc, impl->buckets);
    }
    impl->buckets      = new_buckets;
    impl->bucket_count = new_count;
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Public API — string-based (backward compatible)                           */
/* -------------------------------------------------------------------------- */

cobalt_hashmap_t *cobalt_hashmap_create(size_t initial_buckets)
{
    cobalt_hashmap_t *map = malloc(sizeof(cobalt_hashmap_t));
    if (!map) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    map->base            = hashmap_map_vtable;
    hashmap_impl_t *impl = &map->impl;
    if (initial_buckets > 0) {
        impl->buckets = calloc(initial_buckets, sizeof(hashmap_node_t *));
        if (!impl->buckets) {
            free(map);
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return NULL;
        }
        impl->bucket_count = initial_buckets;
    } else {
        impl->buckets      = NULL;
        impl->bucket_count = 0;
    }
    impl->size       = 0;
    impl->hash_func  = NULL;
    impl->equal_func = NULL;
    impl->alloc      = cobalt_allocator_get_system();
    return map;
}

cobalt_hashmap_t *cobalt_hashmap_create_with_allocator(size_t              initial_buckets,
                                                       cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_hashmap_t *map = cobalt_hashmap_create(initial_buckets);
    if (map) {
        map->impl.alloc = alloc;
    }
    return map;
}

int cobalt_hashmap_put(cobalt_hashmap_t *map, const char *key, void *value)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    hashmap_impl_t *impl = &map->impl;
    if (impl->bucket_count == 0) {
        if (hashmap_ensure_buckets(impl, 16) != 0) {
            return -1;
        }
    }
    if ((impl->size + 1) * 4 / impl->bucket_count > 3) {
        if (hashmap_ensure_buckets(impl, impl->bucket_count * 2) != 0) {
            return -1;
        }
    }
    size_t       key_len = strlen(key);
    unsigned int idx     = node_hash(impl, key, key_len) % impl->bucket_count;

    hashmap_node_t *node = impl->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            node->value = value;
            return 0;
        }
        node = node->next;
    }

    hashmap_node_t *new_node = malloc(sizeof(hashmap_node_t));
    if (!new_node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    new_node->key       = cobalt_strdup(key);
    new_node->key_len   = key_len;
    new_node->key_owned = 1;
    new_node->value     = value;
    new_node->next      = impl->buckets[idx];
    impl->buckets[idx]  = new_node;
    impl->size++;
    return 0;
}

void *cobalt_hashmap_get(const cobalt_hashmap_t *map, const char *key)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    if (map->impl.bucket_count == 0) {
        cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
        return NULL;
    }
    const hashmap_impl_t *impl = &map->impl;
    unsigned int          idx  = hash_string(key) % impl->bucket_count;

    hashmap_node_t *node = impl->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            return node->value;
        }
        node = node->next;
    }
    cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
    return NULL;
}

int cobalt_hashmap_remove(cobalt_hashmap_t *map, const char *key)
{
    if (!map || !key || map->impl.bucket_count == 0) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    hashmap_impl_t *impl = &map->impl;
    unsigned int    idx  = hash_string(key) % impl->bucket_count;

    hashmap_node_t **prev = &impl->buckets[idx];
    hashmap_node_t  *node = *prev;

    while (node) {
        if (strcmp(node->key, key) == 0) {
            *prev = node->next;
            hashmap_free_node(impl, node);
            impl->size--;
            return 0;
        }
        prev = &node->next;
        node = node->next;
    }
    cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
    return -1;
}

size_t cobalt_hashmap_size(const cobalt_hashmap_t *map)
{
    return map ? map->impl.size : 0;
}

size_t cobalt_hashmap_capacity(const cobalt_hashmap_t *map)
{
    return map ? map->impl.bucket_count : 0;
}

void cobalt_hashmap_destroy(cobalt_hashmap_t *map)
{
    if (!map) {
        return;
    }
    hashmap_impl_t *impl = &map->impl;
    if (impl->buckets) {
        for (size_t i = 0; i < impl->bucket_count; i++) {
            hashmap_node_t *node = impl->buckets[i];
            while (node) {
                hashmap_node_t *next = node->next;
                hashmap_free_node(impl, node);
                node = next;
            }
        }
        impl->alloc->free(impl->alloc, impl->buckets);
    }
    free(map);
}

/* -------------------------------------------------------------------------- */
/* Public API — ext (generic keys)                                           */
/* -------------------------------------------------------------------------- */

cobalt_hashmap_t *cobalt_hashmap_create_ext(size_t              initial_buckets,
                                            cobalt_hash_func_t  hash_func,
                                            cobalt_equal_func_t equal_func)
{
    cobalt_hashmap_t *map = cobalt_hashmap_create(initial_buckets);
    if (!map) {
        return NULL;
    }
    map->impl.hash_func  = hash_func;
    map->impl.equal_func = equal_func;
    return map;
}

int cobalt_hashmap_put_ext(cobalt_hashmap_t *map, const void *key, size_t key_len, void *value)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    hashmap_impl_t *impl = &map->impl;
    if (impl->bucket_count == 0) {
        if (hashmap_ensure_buckets(impl, 16) != 0) {
            return -1;
        }
    }
    if ((impl->size + 1) * 4 / impl->bucket_count > 3) {
        if (hashmap_ensure_buckets(impl, impl->bucket_count * 2) != 0) {
            return -1;
        }
    }
    unsigned int idx = node_hash(impl, key, key_len) % impl->bucket_count;

    hashmap_node_t *node = impl->buckets[idx];
    while (node) {
        if (node_equal(impl, node, key, key_len)) {
            node->value = value;
            return 0;
        }
        node = node->next;
    }

    hashmap_node_t *new_node = malloc(sizeof(hashmap_node_t));
    if (!new_node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    new_node->key       = (void *)key;
    new_node->key_len   = key_len;
    new_node->key_owned = 0;
    new_node->value     = value;
    new_node->next      = impl->buckets[idx];
    impl->buckets[idx]  = new_node;
    impl->size++;
    return 0;
}

void *cobalt_hashmap_get_ext(const cobalt_hashmap_t *map, const void *key, size_t key_len)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    if (map->impl.bucket_count == 0) {
        cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
        return NULL;
    }
    const hashmap_impl_t *impl = &map->impl;
    unsigned int          idx  = node_hash(impl, key, key_len) % impl->bucket_count;

    hashmap_node_t *node = impl->buckets[idx];
    while (node) {
        if (node_equal(impl, node, key, key_len)) {
            return node->value;
        }
        node = node->next;
    }
    cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
    return NULL;
}

int cobalt_hashmap_remove_ext(cobalt_hashmap_t *map, const void *key, size_t key_len)
{
    if (!map || !key || map->impl.bucket_count == 0) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    hashmap_impl_t *impl = &map->impl;
    unsigned int    idx  = node_hash(impl, key, key_len) % impl->bucket_count;

    hashmap_node_t **prev = &impl->buckets[idx];
    hashmap_node_t  *node = *prev;

    while (node) {
        if (node_equal(impl, node, key, key_len)) {
            *prev = node->next;
            hashmap_free_node(impl, node);
            impl->size--;
            return 0;
        }
        prev = &node->next;
        node = node->next;
    }
    cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
    return -1;
}
