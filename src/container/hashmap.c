/**
 * @file hashmap.c
 * @brief Hash map (dictionary) container implementation
 * @details Implements a hash map based on string keys. Uses the FNV-1a algorithm for hash
 * calculation and separate chaining for hash collision resolution.
 */

#include "cobalt/container/hashmap.h"
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
    void                *key;   /* Owned key (copied for string maps, referenced for ext) */
    void                *value;
    struct hashmap_node *next;
    size_t               key_len; /* Length of key in bytes (0 for string keys) */
    int                  key_owned; /* 1 = key was strdup'd, must free; 0 = borrowed pointer */
} hashmap_node_t;

typedef struct {
    hashmap_node_t   **buckets;
    size_t             bucket_count;
    size_t             size;
    cobalt_hash_func_t hash_func;    /* NULL = default FNV-1a string hash */
    cobalt_equal_func_t equal_func;  /* NULL = default strcmp */
} hashmap_impl_t;

struct cobalt_hashmap {
    hashmap_impl_t impl;
};

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* -------------------------------------------------------------------------- */

static unsigned int node_hash(const hashmap_impl_t *impl, const void *key, size_t key_len)
{
    if (impl->hash_func) {
        return impl->hash_func(key, key_len);
    }
    /* Default: FNV-1a on C-string */
    return hash_string((const char *)key);
}

static int node_equal(const hashmap_impl_t *impl,
                      const hashmap_node_t *node,
                      const void *key, size_t key_len)
{
    if (impl->equal_func) {
        return impl->equal_func(key, node->key, key_len) != 0;
    }
    /* Default: strcmp */
    return strcmp((const char *)key, (const char *)node->key) == 0;
}

static int hashmap_ensure_buckets(hashmap_impl_t *impl, size_t min_buckets)
{
    if (impl->bucket_count >= min_buckets) {
        return 0;
    }
    size_t new_count = min_buckets > 0 ? min_buckets : 16;
    hashmap_node_t **new_buckets = calloc(new_count, sizeof(hashmap_node_t *));
    if (!new_buckets) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    if (impl->buckets) {
        for (size_t i = 0; i < impl->bucket_count; i++) {
            hashmap_node_t *node = impl->buckets[i];
            while (node) {
                hashmap_node_t *next = node->next;
                unsigned int new_idx = node_hash(impl, node->key, node->key_len) % new_count;
                node->next           = new_buckets[new_idx];
                new_buckets[new_idx] = node;
                node = next;
            }
        }
        free(impl->buckets);
    }
    impl->buckets      = new_buckets;
    impl->bucket_count = new_count;
    return 0;
}

static void hashmap_free_node(hashmap_impl_t *impl, hashmap_node_t *node)
{
    if (node->key_owned) {
        free(node->key);
    }
    free(node);
    (void)impl;
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
    impl->size        = 0;
    impl->hash_func   = NULL;
    impl->equal_func  = NULL;
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
        if (hashmap_ensure_buckets(impl, 16) != 0) return -1;
    }
    if ((impl->size + 1) * 4 / impl->bucket_count > 3) {
        if (hashmap_ensure_buckets(impl, impl->bucket_count * 2) != 0) return -1;
    }

    unsigned int idx = hash_string(key) % impl->bucket_count;

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
    new_node->key    = cobalt_strdup(key);
    if (!new_node->key) {
        free(new_node);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    new_node->key_len     = 0;
    new_node->key_owned   = 1;
    new_node->value       = value;
    new_node->next        = impl->buckets[idx];
    impl->buckets[idx]    = new_node;
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
    unsigned int idx = hash_string(key) % impl->bucket_count;

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
    unsigned int idx = hash_string(key) % impl->bucket_count;

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
    if (!map) return;
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
        free(impl->buckets);
    }
    free(map);
}

/* -------------------------------------------------------------------------- */
/* Public API — ext (generic keys)                                           */
/* -------------------------------------------------------------------------- */

cobalt_hashmap_t *cobalt_hashmap_create_ext(size_t initial_buckets,
                                            cobalt_hash_func_t hash_func,
                                            cobalt_equal_func_t equal_func)
{
    cobalt_hashmap_t *map = cobalt_hashmap_create(initial_buckets);
    if (!map) return NULL;
    map->impl.hash_func = hash_func;
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
        if (hashmap_ensure_buckets(impl, 16) != 0) return -1;
    }
    if ((impl->size + 1) * 4 / impl->bucket_count > 3) {
        if (hashmap_ensure_buckets(impl, impl->bucket_count * 2) != 0) return -1;
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
    new_node->key_owned = 0; /* caller owns the key */
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
    unsigned int idx = node_hash(impl, key, key_len) % impl->bucket_count;

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
    unsigned int idx = node_hash(impl, key, key_len) % impl->bucket_count;

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
