#include "cobalt/container/hashmap.h"
#include <stdlib.h>
#include <string.h>

typedef struct hashmap_node {
    char *key;
    void *value;
    struct hashmap_node *next;
} hashmap_node_t;

typedef struct {
    cobalt_map_t base;
    hashmap_node_t **buckets;
    size_t bucket_count;
    size_t size;
} hashmap_impl_t;

struct cobalt_hashmap {
    hashmap_impl_t impl;
};

static unsigned int hash_string(const char *str) {
    unsigned int h = 0;
    while (*str) {
        h = (h << 5) + h + *str;
        str++;
    }
    return h;
}

/* Map interface operations */
static void *hashmap_get_map(cobalt_map_t *self, const void *key) {
    hashmap_impl_t *impl = (hashmap_impl_t *)self;
    const char *k = (const char *)key;
    unsigned int idx = hash_string(k) % impl->bucket_count;
    
    hashmap_node_t *node = impl->buckets[idx];
    while (node) {
        if (strcmp(node->key, k) == 0) return node->value;
        node = node->next;
    }
    return NULL;
}

static int hashmap_put_map(cobalt_map_t *self, const void *key, void *value) {
    hashmap_impl_t *impl = (hashmap_impl_t *)self;
    const char *k = (const char *)key;
    unsigned int idx = hash_string(k) % impl->bucket_count;
    
    hashmap_node_t *node = impl->buckets[idx];
    while (node) {
        if (strcmp(node->key, k) == 0) {
            node->value = value;
            return 0;
        }
        node = node->next;
    }
    
    hashmap_node_t *new_node = malloc(sizeof(hashmap_node_t));
    if (!new_node) return -1;
    new_node->key = strdup(k);
    new_node->value = value;
    new_node->next = impl->buckets[idx];
    impl->buckets[idx] = new_node;
    impl->size++;
    return 0;
}

static int hashmap_remove_map(cobalt_map_t *self, const void *key) {
    hashmap_impl_t *impl = (hashmap_impl_t *)self;
    const char *k = (const char *)key;
    unsigned int idx = hash_string(k) % impl->bucket_count;
    
    hashmap_node_t **prev = &impl->buckets[idx];
    hashmap_node_t *node = *prev;
    
    while (node) {
        if (strcmp(node->key, k) == 0) {
            *prev = node->next;
            free(node->key);
            free(node);
            impl->size--;
            return 0;
        }
        prev = &node->next;
        node = node->next;
    }
    return -1;
}

static size_t hashmap_size_map(cobalt_map_t *self) {
    hashmap_impl_t *impl = (hashmap_impl_t *)self;
    return impl->size;
}

static int hashmap_is_empty_map(cobalt_map_t *self) {
    hashmap_impl_t *impl = (hashmap_impl_t *)self;
    return impl->size == 0;
}

cobalt_hashmap_t *cobalt_hashmap_create(size_t initial_buckets) {
    cobalt_hashmap_t *map = malloc(sizeof(cobalt_hashmap_t));
    if (!map) return NULL;
    
    hashmap_impl_t *impl = &map->impl;
    size_t buckets = initial_buckets > 0 ? initial_buckets : 16;
    impl->buckets = calloc(buckets, sizeof(hashmap_node_t*));
    if (!impl->buckets) {
        free(map);
        return NULL;
    }
    
    impl->bucket_count = buckets;
    impl->size = 0;
    
    /* Initialize map interface */
    impl->base.get = hashmap_get_map;
    impl->base.put = hashmap_put_map;
    impl->base.remove = hashmap_remove_map;
    impl->base.size = hashmap_size_map;
    impl->base.is_empty = hashmap_is_empty_map;
    
    return map;
}

void cobalt_hashmap_destroy(cobalt_hashmap_t *map) {
    if (!map) return;
    
    hashmap_impl_t *impl = &map->impl;
    for (size_t i = 0; i < impl->bucket_count; i++) {
        hashmap_node_t *node = impl->buckets[i];
        while (node) {
            hashmap_node_t *next = node->next;
            free(node->key);
            free(node);
            node = next;
        }
    }
    free(impl->buckets);
    free(map);
}

int cobalt_hashmap_put(cobalt_hashmap_t *map, const char *key, void *value) {
    if (!map || !key) return -1;
    return map->impl.base.put((cobalt_map_t *)map, key, value);
}

void *cobalt_hashmap_get(cobalt_hashmap_t *map, const char *key) {
    if (!map || !key) return NULL;
    return map->impl.base.get((cobalt_map_t *)map, key);
}

int cobalt_hashmap_remove(cobalt_hashmap_t *map, const char *key) {
    if (!map || !key) return -1;
    return map->impl.base.remove((cobalt_map_t *)map, key);
}

size_t cobalt_hashmap_size(cobalt_hashmap_t *map) {
    if (!map) return 0;
    return cobalt_hashmap_size(map);
}
