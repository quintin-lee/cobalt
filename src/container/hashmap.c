#include "cobalt/container/hashmap.h"
#include <stdlib.h>
#include <string.h>

typedef struct hashmap_node {
    char *key;
    void *value;
    struct hashmap_node *next;
} hashmap_node_t;

typedef struct {
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
    hashmap_impl_t *impl = &map->impl;
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
    if (!new_node) return -1;
    new_node->key = strdup(key);
    new_node->value = value;
    new_node->next = impl->buckets[idx];
    impl->buckets[idx] = new_node;
    impl->size++;
    return 0;
}

void *cobalt_hashmap_get(cobalt_hashmap_t *map, const char *key) {
    if (!map || !key) return NULL;
    hashmap_impl_t *impl = &map->impl;
    unsigned int idx = hash_string(key) % impl->bucket_count;
    
    hashmap_node_t *node = impl->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) return node->value;
        node = node->next;
    }
    return NULL;
}

int cobalt_hashmap_remove(cobalt_hashmap_t *map, const char *key) {
    if (!map || !key) return -1;
    hashmap_impl_t *impl = &map->impl;
    unsigned int idx = hash_string(key) % impl->bucket_count;
    
    hashmap_node_t **prev = &impl->buckets[idx];
    hashmap_node_t *node = *prev;
    
    while (node) {
        if (strcmp(node->key, key) == 0) {
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

size_t cobalt_hashmap_size(cobalt_hashmap_t *map) {
    if (!map) return 0;
    return map->impl.size;
}
