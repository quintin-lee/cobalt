#include "cobalt/container/hashmap.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>
#include <string.h>

/* Portable strdup for C11 */
static char* my_strdup(const char* s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s) + 1;
    char* dup = malloc(len);
    if (dup)
        memcpy(dup, s, len);
    return dup;
}

typedef struct hashmap_node
{
    char* key;
    void* value;
    struct hashmap_node* next;
} hashmap_node_t;

typedef struct
{
    hashmap_node_t** buckets;
    size_t bucket_count;
    size_t size;
} hashmap_impl_t;

struct cobalt_hashmap
{
    hashmap_impl_t impl;
};

static unsigned int hash_string(const char* str)
{
    unsigned int h = 2166136261U;
    while (*str)
        {
            h = (h ^ (unsigned char)*str) * 16777619U;
            str++;
        }
    return h;
}

cobalt_hashmap_t* cobalt_hashmap_create(size_t initial_buckets)
{
    cobalt_hashmap_t* map = malloc(sizeof(cobalt_hashmap_t));
    if (!map)
        {
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return NULL;
        }

    hashmap_impl_t* impl = &map->impl;
    if (initial_buckets > 0)
        {
            impl->buckets = calloc(initial_buckets, sizeof(hashmap_node_t*));
            if (!impl->buckets)
                {
                    free(map);
                    cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
                    return NULL;
                }
            impl->bucket_count = initial_buckets;
        }
    else
        {
            impl->buckets = NULL;
            impl->bucket_count = 0;
        }
    impl->size = 0;
    return map;
}

static int hashmap_ensure_buckets(hashmap_impl_t* impl, size_t min_buckets)
{
    if (impl->bucket_count >= min_buckets)
        return 0;
    size_t new_count = min_buckets > 0 ? min_buckets : 16;
    hashmap_node_t** new_buckets = calloc(new_count, sizeof(hashmap_node_t*));
    if (!new_buckets)
        {
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return -1;
        }

    if (impl->buckets)
        {
            for (size_t i = 0; i < impl->bucket_count; i++)
                {
                    hashmap_node_t* node = impl->buckets[i];
                    while (node)
                        {
                            hashmap_node_t* next = node->next;
                            unsigned int new_idx = hash_string(node->key) % new_count;
                            node->next = new_buckets[new_idx];
                            new_buckets[new_idx] = node;
                            node = next;
                        }
                }
            free(impl->buckets);
        }
    impl->buckets = new_buckets;
    impl->bucket_count = new_count;
    return 0;
}

void cobalt_hashmap_destroy(cobalt_hashmap_t* map)
{
    if (!map)
        return;
    hashmap_impl_t* impl = &map->impl;
    for (size_t i = 0; i < impl->bucket_count; i++)
        {
            hashmap_node_t* node = impl->buckets[i];
            while (node)
                {
                    hashmap_node_t* next = node->next;
                    free(node->key);
                    free(node);
                    node = next;
                }
        }
    free(impl->buckets);
    free(map);
}

int cobalt_hashmap_put(cobalt_hashmap_t* map, const char* key, void* value)
{
    if (!map || !key)
        {
            cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
            return -1;
        }
    hashmap_impl_t* impl = &map->impl;

    if (impl->bucket_count == 0)
        {
            if (hashmap_ensure_buckets(impl, 16) != 0)
                return -1;
        }

    if ((impl->size + 1) * 4 / impl->bucket_count > 3)
        {
            if (hashmap_ensure_buckets(impl, impl->bucket_count * 2) != 0)
                return -1;
        }

    unsigned int idx = hash_string(key) % impl->bucket_count;

    hashmap_node_t* node = impl->buckets[idx];
    while (node)
        {
            if (strcmp(node->key, key) == 0)
                {
                    node->value = value;
                    return 0;
                }
            node = node->next;
        }

    hashmap_node_t* new_node = malloc(sizeof(hashmap_node_t));
    if (!new_node)
        {
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return -1;
        }
    new_node->key = my_strdup(key);
    if (!new_node->key)
        {
            free(new_node);
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return -1;
        }
    new_node->value = value;
    new_node->next = impl->buckets[idx];
    impl->buckets[idx] = new_node;
    impl->size++;
    return 0;
}

void* cobalt_hashmap_get(cobalt_hashmap_t* map, const char* key)
{
    if (!map || !key)
        {
            cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
            return NULL;
        }
    if (map->impl.bucket_count == 0)
        {
            cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
            return NULL;
        }
    hashmap_impl_t* impl = &map->impl;
    unsigned int idx = hash_string(key) % impl->bucket_count;

    hashmap_node_t* node = impl->buckets[idx];
    while (node)
        {
            if (strcmp(node->key, key) == 0)
                return node->value;
            node = node->next;
        }
    cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
    return NULL;
}

int cobalt_hashmap_remove(cobalt_hashmap_t* map, const char* key)
{
    if (!map || !key || map->impl.bucket_count == 0)
        {
            cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
            return -1;
        }
    hashmap_impl_t* impl = &map->impl;
    unsigned int idx = hash_string(key) % impl->bucket_count;

    hashmap_node_t** prev = &impl->buckets[idx];
    hashmap_node_t* node = *prev;

    while (node)
        {
            if (strcmp(node->key, key) == 0)
                {
                    *prev = node->next;
                    free(node->key);
                    free(node);
                    impl->size--;
                    return 0;
                }
            prev = &node->next;
            node = node->next;
        }
    cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
    return -1;
}

size_t cobalt_hashmap_size(cobalt_hashmap_t* map)
{
    if (!map)
        return 0;
    return map->impl.size;
}

size_t cobalt_hashmap_capacity(const cobalt_hashmap_t* map)
{
    if (!map)
        return 0;
    return map->impl.bucket_count;
}
