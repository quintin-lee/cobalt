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

/**
 * @brief Hash map node structure used internally
 */
typedef struct hashmap_node {
    char                *key;   /* Deep copy of the key */
    void                *value; /* Value pointer */
    struct hashmap_node *next;  /* Pointer to the next node */
} hashmap_node_t;

/**
 * @brief Hash map internal implementation data structure
 */
typedef struct {
    hashmap_node_t   *
        *buckets;      /* Bucket array, each element is a head pointer to a singly linked list */
    size_t bucket_count; /* Number of buckets (array size) */
    size_t size;         /* Current total number of stored key-value pairs */
} hashmap_impl_t;

/**
 * @brief Opaque hash map node structure
 */
struct cobalt_hashmap_node {
    char               *key;
    void               *value;
    cobalt_hashmap_node_t *next;
};

/**
 * @brief Hash map type definition, encapsulating the specific implementation
 */
struct cobalt_hashmap {
    hashmap_impl_t impl; /* Specific hash map implementation data */
};

/**
 * @brief Calculate the hash value of a string
 * @details Uses the FNV-1a (Fowler-Noll-Vo) hash algorithm, known for being simple and providing
 * good dispersion, suitable for strings.
 *
 * @param str The C string to calculate the hash for
 * @return The calculated 32-bit unsigned hash value
 */
static unsigned int hash_string(const char *str)
{
    unsigned int h = 2166136261U; // FNV offset basis
    while (*str) {
        h = (h ^ (unsigned char)*str) * 16777619U; // FNV prime
        str++;
    }
    return h;
}

/**
 * @brief Create a new hash map
 * @details Allocates the initial bucket array based on initial_buckets. If the initial number of
 * buckets is 0, allocation is deferred until the first put.
 *
 * @param initial_buckets Capacity of the initial bucket array
 * @return Returns the newly allocated hash map, or NULL if out of memory, setting
 * COBALT_ERROR_OUT_OF_MEMORY.
 */
cobalt_hashmap_t *cobalt_hashmap_create(size_t initial_buckets)
{
    cobalt_hashmap_t *map = malloc(sizeof(cobalt_hashmap_t));
    if (!map) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    hashmap_impl_t *impl = &map->impl;
    if (initial_buckets > 0) {
        // Allocate and zero the bucket array, ensuring all linked list heads are initialized to
        // NULL
        impl->buckets = calloc(initial_buckets, sizeof(hashmap_node_t *));
        if (!impl->buckets) {
            free(map);
            cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
            return NULL;
        }
        impl->bucket_count = initial_buckets;
    } else {
        // Lazy loading: do not allocate bucket memory initially
        impl->buckets      = NULL;
        impl->bucket_count = 0;
    }
    impl->size = 0;
    return map;
}

/**
 * @brief Ensure the hash map has the specified minimum number of buckets, resizing and rehashing
 * when necessary
 * @details If the current number of buckets is less than the minimum requirement, a larger bucket
 * array is reallocated, and all existing nodes are remapped into the new buckets.
 *
 * @param impl Hash map internal implementation pointer
 * @param min_buckets The minimum number of buckets required
 * @return Returns 0 on success, -1 if memory allocation fails
 */
static int hashmap_ensure_buckets(hashmap_impl_t *impl, size_t min_buckets)
{
    // If current capacity already meets the requirement, return directly
    if (impl->bucket_count >= min_buckets) {
        return 0;
    }
    // Default minimum expansion size is 16
    size_t           new_count   = min_buckets > 0 ? min_buckets : 16;
    hashmap_node_t **new_buckets = calloc(new_count, sizeof(hashmap_node_t *));
    if (!new_buckets) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    // If there were existing buckets, perform rehashing
    if (impl->buckets) {
        for (size_t i = 0; i < impl->bucket_count; i++) {
            hashmap_node_t *node = impl->buckets[i];
            while (node) {
                hashmap_node_t *next = node->next; // Save the next node
                unsigned int    new_idx =
                    hash_string(node->key) % new_count; // Calculate the index in the new bucket

                // Insert the current node using head insertion into the linked list of the new
                // bucket
                node->next           = new_buckets[new_idx];
                new_buckets[new_idx] = node;

                node = next; // Continue processing the next node
            }
        }
        free(impl->buckets); // Free the old bucket array
    }
    impl->buckets      = new_buckets;
    impl->bucket_count = new_count;
    return 0;
}

/**
 * @brief Destroy the hash map and free all related memory
 * @details Iterates through all buckets and linked list nodes, frees key copies and the nodes
 * themselves, and finally frees the bucket array and the hash map structure.
 *
 * @param map The hash map to be destroyed
 */
void cobalt_hashmap_destroy(cobalt_hashmap_t *map)
{
    if (!map) {
        return;
    }
    hashmap_impl_t *impl = &map->impl;
    for (size_t i = 0; i < impl->bucket_count; i++) {
        hashmap_node_t *node = impl->buckets[i];
        while (node) {
            hashmap_node_t *next = node->next;
            free(node->key); // Free the key copy allocated by cobalt_strdup
            free(node);      // Free the node
            node = next;
        }
    }
    free(impl->buckets);
    free(map);
}

/**
 * @brief Insert or update a key-value pair
 * @details If the key already exists, its corresponding value is updated. If it does not exist, a
 * new node is allocated for insertion. When the load factor exceeds 0.75, automatic rehashing
 * (expansion) is triggered.
 *
 * @param map The hash map
 * @param key The string key
 * @param value The corresponding value
 * @return Returns 0 on success, -1 on failure
 */
int cobalt_hashmap_put(cobalt_hashmap_t *map, const char *key, void *value)
{
    if (!map || !key) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    hashmap_impl_t *impl = &map->impl;

    // If in lazy loading state, initialize to 16 buckets
    if (impl->bucket_count == 0) {
        if (hashmap_ensure_buckets(impl, 16) != 0) {
            return -1;
        }
    }

    // Check load factor: (size + 1) / capacity > 0.75 is equivalent to (size + 1) * 4 / capacity >
    // 3
    if ((impl->size + 1) * 4 / impl->bucket_count > 3) {
        // Double the capacity
        if (hashmap_ensure_buckets(impl, impl->bucket_count * 2) != 0) {
            return -1;
        }
    }

    unsigned int idx = hash_string(key) % impl->bucket_count;

    // Check if the key already exists
    hashmap_node_t *node = impl->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            node->value = value; // Found matching key, update value directly and return
            return 0;
        }
        node = node->next;
    }

    // Key does not exist, create new node
    hashmap_node_t *new_node = malloc(sizeof(hashmap_node_t));
    if (!new_node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    // Deep copy the string key to ensure independent lifecycle
    new_node->key = cobalt_strdup(key);
    if (!new_node->key) {
        free(new_node);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    // Use head insertion to insert the new node at the beginning of the linked list for the
    // corresponding bucket
    new_node->value    = value;
    new_node->next     = impl->buckets[idx];
    impl->buckets[idx] = new_node;
    impl->size++;

    return 0;
}

/**
 * @brief Get the value corresponding to a key
 * @details Calculates the hash value and traverses the linked list in the corresponding bucket to
 * find it.
 *
 * @param map The hash map
 * @param key The key to look up
 * @return Returns the corresponding value if found, NULL if not found.
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
    hashmap_impl_t *impl = &map->impl;
    unsigned int    idx  = hash_string(key) % impl->bucket_count;

    // Search in the linked list of the corresponding bucket
    hashmap_node_t *node = impl->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            return node->value; // Match successful, return value
        }
        node = node->next;
    }

    cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
    return NULL;
}

/**
 * @brief Remove a specified key-value pair from the hash map
 * @details Finds the node for the corresponding key, detaches it from the singly linked list, and
 * frees related memory.
 *
 * @param map The hash map
 * @param key The key to remove
 * @return Returns 0 on successful removal, -1 if the key does not exist or parameters are invalid
 */
int cobalt_hashmap_remove(cobalt_hashmap_t *map, const char *key)
{
    if (!map || !key || map->impl.bucket_count == 0) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    hashmap_impl_t *impl = &map->impl;
    unsigned int    idx  = hash_string(key) % impl->bucket_count;

    // Use a double pointer to simplify node deletion in the singly linked list
    hashmap_node_t **prev = &impl->buckets[idx];
    hashmap_node_t  *node = *prev;

    while (node) {
        if (strcmp(node->key, key) == 0) {
            *prev = node->next; // Detach node
            free(node->key);    // Free key copy
            free(node);         // Free node itself
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
 * @brief Get the number of elements in the hash map
 *
 * @param map The hash map
 * @return Returns the total number of stored key-value pairs
 */
size_t cobalt_hashmap_size(const cobalt_hashmap_t *map)
{
    if (!map) {
        return 0;
    }
    return map->impl.size;
}

/**
 * @brief Get the number of buckets in the hash map
 *
 * @param map The hash map
 * @return Returns the current total number of buckets (capacity)
 */
size_t cobalt_hashmap_capacity(const cobalt_hashmap_t *map)
{
    if (!map) {
        return 0;
    }
    return map->impl.bucket_count;
}
