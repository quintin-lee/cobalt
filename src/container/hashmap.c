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

/* FNV-1a parameters */
enum { COBALT_FNV1A_OFFSET_BASIS = 2166136261U };
enum { COBALT_FNV1A_PRIME = 16777619U };
/* Default bucket count for lazy init */
enum { COBALT_HASHMAP_DEFAULT_BUCKETS = 16 };

/**
 * @brief Round a value up to the next power of two (minimum 2)
 *
 * @param v Value to round up
 * @return Smallest power of two >= v, at least 2
 */
static inline size_t hashmap_next_pow2(size_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
#ifdef __LP64__
    v |= v >> 32;
#endif
    v++;
    return v < 2 ? 2 : v;
}

/* -------------------------------------------------------------------------- */
/* FNV-1a hash for string keys                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Hash a NUL-terminated string with FNV-1a
 *
 * @param str String to hash
 * @return 32-bit FNV-1a digest
 */
static unsigned int hash_string(const char *str)
{
    unsigned int hash_val = COBALT_FNV1A_OFFSET_BASIS;
    while (*str) {
        hash_val = (hash_val ^ (unsigned char)*str) * COBALT_FNV1A_PRIME;
        str++;
    }
    return hash_val;
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
    cobalt_allocator_t *alloc; /* Allocator used for all internal allocations */
} hashmap_impl_t;

struct cobalt_hashmap {
    cobalt_map_t   base;
    hashmap_impl_t impl;
};

/* -------------------------------------------------------------------------- */
/* Map iterator (hashmap-specific)                                           */
/* -------------------------------------------------------------------------- */

typedef struct {
    hashmap_impl_t     *impl;
    hashmap_node_t     *node;
    size_t              bucket_idx;
    cobalt_allocator_t *alloc;
} hashmap_map_iter_t;

/**
 * @brief Check whether the hash map iterator has a current node
 *
 * @param ctx Iterator state
 * @return Non-zero when a current node exists, 0 otherwise
 */
static int hashmap_map_iter_has_next(void *ctx)
{
    hashmap_map_iter_t *iter = (hashmap_map_iter_t *)ctx;
    return iter->node != NULL;
}

/**
 * @brief Return the current key-value pair and advance across buckets
 *
 * @param ctx Iterator state
 * @return Current pair; {NULL, NULL} when exhausted
 */
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

/**
 * @brief Free iterator state via the map allocator
 *
 * @param ctx Iterator state to destroy
 */
static void hashmap_map_iter_destroy(void *ctx)
{
    hashmap_map_iter_t *iter = (hashmap_map_iter_t *)ctx;
    iter->alloc->free(iter->alloc, ctx);
}

static const cobalt_map_iterator_vtable_t hashmap_map_iter_vtable = {
    .has_next = hashmap_map_iter_has_next,
    .next     = hashmap_map_iter_next,
    .destroy  = hashmap_map_iter_destroy,
};

/**
 * @brief Build a Map-interface iterator positioned at the first entry
 *
 * @param self Map instance
 * @return Iterator pointer, or NULL on allocation failure
 */
static cobalt_map_iterator_t *hashmap_map_iterator(cobalt_map_t *self)
{
    cobalt_hashmap_t *map  = (cobalt_hashmap_t *)self;
    hashmap_impl_t   *impl = &map->impl;

    hashmap_map_iter_t *iter_state =
        (hashmap_map_iter_t *)impl->alloc->alloc(impl->alloc, sizeof(hashmap_map_iter_t));
    if (iter_state) {
        memset(iter_state, 0, sizeof(hashmap_map_iter_t));
        iter_state->alloc = impl->alloc;
    }
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

    cobalt_map_iterator_t *iter =
        (cobalt_map_iterator_t *)impl->alloc->alloc(impl->alloc, sizeof(cobalt_map_iterator_t));
    if (!iter) {
        impl->alloc->free(impl->alloc, iter_state);
        return NULL;
    }
    iter->vtable = &hashmap_map_iter_vtable;
    iter->data   = iter_state;
    return iter;
}

/* -------------------------------------------------------------------------- */
/* Map interface function implementations                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Map-slot get delegating to the generic-key lookup
 *
 * @param self Map instance
 * @param key Key pointer
 * @param key_len Key length in bytes
 * @return Value pointer, or NULL when not found
 */
static void *hashmap_map_get(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_get_ext(map, key, key_len);
}

/**
 * @brief Map-slot put delegating to the generic-key insert
 *
 * @param self Map instance
 * @param key Key pointer
 * @param key_len Key length in bytes
 * @param value Value to store
 * @return 0 on success, -1 on failure
 */
static int hashmap_map_put(cobalt_map_t *self, const void *key, size_t key_len, void *value)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_put_ext(map, key, key_len, value);
}

/**
 * @brief Map-slot remove delegating to the generic-key removal
 *
 * @param self Map instance
 * @param key Key pointer
 * @param key_len Key length in bytes
 * @return 0 on success, -1 when not found
 */
static int hashmap_map_remove(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_remove_ext(map, key, key_len);
}

/**
 * @brief Map-slot size query
 *
 * @param self Map instance
 * @return Number of stored entries
 */
static size_t hashmap_map_size(cobalt_map_t *self)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_size(map);
}

/**
 * @brief Map-slot emptiness check
 *
 * @param self Map instance
 * @return Non-zero when empty, 0 otherwise
 */
static int hashmap_map_is_empty(cobalt_map_t *self)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_size(map) == 0;
}

/**
 * @brief Map-slot destroy delegating to the public destructor
 *
 * @param self Map instance to destroy
 */
static void hashmap_map_destroy(cobalt_map_t *self)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    cobalt_hashmap_destroy(map);
}

/**
 * @brief Map-slot contains check via the generic-key lookup
 *
 * @param self Map instance
 * @param key Key pointer
 * @param key_len Key length in bytes
 * @return Non-zero when the key exists, 0 otherwise
 */
static int hashmap_map_contains(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)self;
    return cobalt_hashmap_get_ext(map, key, key_len) != NULL;
}

/**
 * @brief Release a node and its owned key copy
 *
 * @param impl Map internals holding the allocator
 * @param node Node to free; no-op on NULL
 */
static void hashmap_free_node(hashmap_impl_t *impl, hashmap_node_t *node)
{
    if (!impl || !node) {
        return;
    }
    if (node->key_owned && node->key) {
        free(node->key);
    }
    impl->alloc->free(impl->alloc, node);
}

/**
 * @brief Map-slot clear removing all entries but keeping buckets
 *
 * @param self Map instance
 */
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

/**
 * @brief Compute the hash of a key with the custom or default function
 *
 * @param impl Map internals holding the optional custom hash function
 * @param key Key pointer
 * @param key_len Key length in bytes
 * @return 32-bit hash value
 */
static unsigned int node_hash(const hashmap_impl_t *impl, const void *key, size_t key_len)
{
    if (impl->hash_func) {
        return impl->hash_func(key, key_len);
    }
    return hash_string((const char *)key);
}

/**
 * @brief Compare a lookup key against a stored node key
 *
 * @param impl Map internals holding the optional custom equality function
 * @param node Stored node to compare against
 * @param key Lookup key pointer
 * @param key_len Key length in bytes
 * @return Non-zero when keys match, 0 otherwise
 */
static int
node_equal(const hashmap_impl_t *impl, const hashmap_node_t *node, const void *key, size_t key_len)
{
    if (impl->equal_func) {
        return impl->equal_func(key, node->key, key_len) != 0;
    }
    return strcmp((const char *)key, (const char *)node->key) == 0;
}

/**
 * @brief Grow the bucket array to at least min_buckets and rehash entries
 *
 * @param impl Map internals to grow
 * @param min_buckets Minimum bucket count (rounded up to a power of two)
 * @return 0 on success, -1 on allocation failure
 * @note Bucket count stays a power of two so callers can use bitmask indexing
 */
static int hashmap_ensure_buckets(hashmap_impl_t *impl, size_t min_buckets)
{
    if (impl->bucket_count >= min_buckets) {
        return 0;
    }
    size_t new_count = min_buckets > 0 ? min_buckets : COBALT_HASHMAP_DEFAULT_BUCKETS;
    /* Round up to next power of 2 for fast bitmask indexing */
    new_count       = hashmap_next_pow2(new_count);
    void *tmp_alloc = impl->alloc->alloc(impl->alloc, new_count * sizeof(hashmap_node_t *));
    hashmap_node_t **new_buckets = (hashmap_node_t **)tmp_alloc;
    if (!new_buckets) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    for (size_t _i = 0; _i < new_count; _i++) {
        new_buckets[_i] = NULL;
    }
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
        impl->alloc->free(impl->alloc, (void *)impl->buckets);
    }
    impl->buckets      = new_buckets;
    impl->bucket_count = new_count;
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Public API — string-based (backward compatible)                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create a hash map with string keys and lazy bucket allocation
 *
 * @param initial_buckets Initial bucket count (rounded up to power of two); 0 defers allocation
 * @return New map pointer, or NULL on allocation failure
 */
cobalt_hashmap_t *cobalt_hashmap_create(size_t initial_buckets)
{
    cobalt_allocator_t *alloc = cobalt_allocator_get_system();
    cobalt_hashmap_t   *map   = (cobalt_hashmap_t *)alloc->alloc(alloc, sizeof(cobalt_hashmap_t));
    if (!map) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    map->base            = hashmap_map_vtable;
    hashmap_impl_t *impl = &map->impl;
    impl->alloc          = alloc;
    if (initial_buckets > 0) {
        size_t pow2_count = hashmap_next_pow2(initial_buckets);
        impl->buckets =
            (hashmap_node_t **)alloc->alloc(alloc, sizeof(hashmap_node_t *) * pow2_count);
        if (impl->buckets) {
            memset(impl->buckets, 0, sizeof(hashmap_node_t *) * pow2_count);
        }
        if (!impl->buckets) {
            alloc->free(alloc, map);
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return NULL;
        }
        impl->bucket_count = pow2_count;
    } else {
        impl->buckets      = NULL;
        impl->bucket_count = 0;
    }
    impl->size       = 0;
    impl->hash_func  = NULL;
    impl->equal_func = NULL;
    return map;
}

/**
 * @brief Create a hash map using a custom allocator
 *
 * @param initial_buckets Initial bucket count (rounded up to power of two); 0 defers allocation
 * @param alloc Custom allocator, must not be NULL
 * @return New map pointer, or NULL on failure
 */
cobalt_hashmap_t *cobalt_hashmap_create_with_allocator(size_t              initial_buckets,
                                                       cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_hashmap_t *map = (cobalt_hashmap_t *)alloc->alloc(alloc, sizeof(cobalt_hashmap_t));
    if (!map) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    map->base            = hashmap_map_vtable;
    hashmap_impl_t *impl = &map->impl;
    impl->alloc          = alloc;
    if (initial_buckets > 0) {
        size_t pow2_count = hashmap_next_pow2(initial_buckets);
        impl->buckets =
            (hashmap_node_t **)alloc->alloc(alloc, sizeof(hashmap_node_t *) * pow2_count);
        if (impl->buckets) {
            memset(impl->buckets, 0, sizeof(hashmap_node_t *) * pow2_count);
        }
        if (!impl->buckets) {
            alloc->free(alloc, map);
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return NULL;
        }
        impl->bucket_count = pow2_count;
    } else {
        impl->buckets      = NULL;
        impl->bucket_count = 0;
    }
    impl->size       = 0;
    impl->hash_func  = NULL;
    impl->equal_func = NULL;
    return map;
}

/**
 * @brief Insert or replace a string-key entry, growing past load factor 0.75
 *
 * @param map Hash map instance
 * @param key NUL-terminated string key (copied internally)
 * @param value Value to store
 * @return 0 on success, -1 on invalid arguments or allocation failure
 */
int cobalt_hashmap_put(cobalt_hashmap_t *map, const char *key, void *value)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    hashmap_impl_t *impl = &map->impl;
    if (impl->bucket_count == 0) {
        if (hashmap_ensure_buckets(impl, COBALT_HASHMAP_DEFAULT_BUCKETS) != 0) {
            return -1;
        }
    }
    if ((impl->size + 1) * 4 / impl->bucket_count > 3) {
        if (hashmap_ensure_buckets(impl, impl->bucket_count << 1) != 0) {
            return -1;
        }
    }
    size_t       key_len = strlen(key);
    unsigned int idx     = (unsigned int)(node_hash(impl, key, key_len) & (impl->bucket_count - 1));

    hashmap_node_t *node = impl->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            node->value = value;
            return 0;
        }
        node = node->next;
    }

    hashmap_node_t *new_node =
        (hashmap_node_t *)impl->alloc->alloc(impl->alloc, sizeof(hashmap_node_t));
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

/**
 * @brief Look up a value by string key
 *
 * @param map Hash map instance
 * @param key NUL-terminated string key
 * @return Value pointer, or NULL when missing or on invalid arguments
 */
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
    const hashmap_impl_t *impl    = &map->impl;
    size_t                key_len = strlen(key);
    unsigned int idx = (unsigned int)(node_hash(impl, key, key_len) & (impl->bucket_count - 1));

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

/**
 * @brief Remove a string-key entry and free its key copy
 *
 * @param map Hash map instance
 * @param key NUL-terminated string key
 * @return 0 on success, -1 when not found or on invalid arguments
 */
int cobalt_hashmap_remove(cobalt_hashmap_t *map, const char *key)
{
    if (!map || !key || map->impl.bucket_count == 0) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    hashmap_impl_t *impl    = &map->impl;
    size_t          key_len = strlen(key);
    unsigned int    idx = (unsigned int)(node_hash(impl, key, key_len) & (impl->bucket_count - 1));

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

/**
 * @brief Query the number of stored entries
 *
 * @param map Hash map instance
 * @return Entry count, or 0 for NULL map
 */
size_t cobalt_hashmap_size(const cobalt_hashmap_t *map)
{
    return map ? map->impl.size : 0;
}

/**
 * @brief Query the current bucket count
 *
 * @param map Hash map instance
 * @return Allocated bucket count, or 0 for NULL map
 */
size_t cobalt_hashmap_capacity(const cobalt_hashmap_t *map)
{
    return map ? map->impl.bucket_count : 0;
}

/**
 * @brief Destroy the map freeing nodes, key copies and buckets
 *
 * @param map Hash map to destroy; no-op on NULL
 * @note Stored values are not freed
 */
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
        impl->alloc->free(impl->alloc, (void *)impl->buckets);
    }
    impl->alloc->free(impl->alloc, map);
}

/* -------------------------------------------------------------------------- */
/* Public API — ext (generic keys)                                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create a hash map with custom hash and equality callbacks
 *
 * @param initial_buckets Initial bucket count; 0 defers allocation
 * @param hash_func Custom hash function (NULL selects FNV-1a string hash)
 * @param equal_func Custom equality function (NULL selects strcmp)
 * @return New map pointer, or NULL on allocation failure
 * @note With custom callbacks keys are referenced, not copied
 */
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

/**
 * @brief Replace the hash and equality callbacks of an existing map
 *
 * @param map Hash map instance
 * @param hash_func New hash function (NULL selects the default)
 * @param equal_func New equality function (NULL selects the default)
 * @return 0 on success, -1 for NULL map
 * @note Applies to future operations only; existing entries are not rehashed
 */
int cobalt_hashmap_set_funcs(cobalt_hashmap_t   *map,
                             cobalt_hash_func_t  hash_func,
                             cobalt_equal_func_t equal_func)
{
    if (!map) {
        return -1;
    }
    map->impl.hash_func  = hash_func;
    map->impl.equal_func = equal_func;
    return 0;
}

/**
 * @brief Insert or replace a generic-key entry, growing past load factor 0.75
 *
 * @param map Hash map created with cobalt_hashmap_create_ext()
 * @param key Opaque key pointer (referenced, not copied)
 * @param key_len Key length in bytes
 * @param value Value to store
 * @return 0 on success, -1 on invalid arguments or allocation failure
 */
int cobalt_hashmap_put_ext(cobalt_hashmap_t *map, const void *key, size_t key_len, void *value)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    hashmap_impl_t *impl = &map->impl;
    if (impl->bucket_count == 0) {
        if (hashmap_ensure_buckets(impl, COBALT_HASHMAP_DEFAULT_BUCKETS) != 0) {
            return -1;
        }
    }
    if ((impl->size + 1) * 4 / impl->bucket_count > 3) {
        if (hashmap_ensure_buckets(impl, impl->bucket_count << 1) != 0) {
            return -1;
        }
    }
    unsigned int idx = (unsigned int)(node_hash(impl, key, key_len) & (impl->bucket_count - 1));

    hashmap_node_t *node = impl->buckets[idx];
    while (node) {
        if (node_equal(impl, node, key, key_len)) {
            node->value = value;
            return 0;
        }
        node = node->next;
    }

    hashmap_node_t *new_node =
        (hashmap_node_t *)impl->alloc->alloc(impl->alloc, sizeof(hashmap_node_t));
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

/**
 * @brief Look up a value by generic key
 *
 * @param map Hash map created with cobalt_hashmap_create_ext()
 * @param key Opaque key pointer
 * @param key_len Key length in bytes
 * @return Value pointer, or NULL when missing or on invalid arguments
 */
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
    unsigned int idx = (unsigned int)(node_hash(impl, key, key_len) & (impl->bucket_count - 1));

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

/**
 * @brief Remove a binary-key entry from the hash map
 * @param map Hash map pointer
 * @param key Pointer to the binary key bytes
 * @param key_len Length of the key in bytes
 * @return 0 on success, -1 on invalid arguments or when the key is not found
 *
 * The bucket index uses a bitmask over the node hash; bucket_count is always
 * a power of two, so this stays consistent with put/get lookups.
 */
int cobalt_hashmap_remove_ext(cobalt_hashmap_t *map, const void *key, size_t key_len)
{
    if (!map || !key || map->impl.bucket_count == 0) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    hashmap_impl_t *impl = &map->impl;
    unsigned int    idx  = node_hash(impl, key, key_len) & (impl->bucket_count - 1);

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
