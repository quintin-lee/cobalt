#ifndef HASHMAP_H
#define HASHMAP_H

/**
 * @file hashmap.h
 * @brief Hash map (dictionary) container
 * @details Provides a hash map implementation based on string keys (C-strings), using separate
 * chaining for hash collision resolution.
 */

#include <stddef.h>

/**
 * @defgroup hashmap Hash Map (HashMap)
 * @brief Hash map implementation mapping key-value pairs
 * @{
 */

/**
 * @brief Hash map node type definition
 */
typedef struct cobalt_hashmap_node cobalt_hashmap_node_t;

/**
 * @brief Hash map node structure
 * @details Linked list node used to resolve collisions via separate chaining within the same
 * bucket.
 */
struct cobalt_hashmap_node {
    const char *key;   /**< The string key of the node */
    void       *value; /**< Pointer to the value stored in the node */
    cobalt_hashmap_node_t
        *next; /**< Pointer to the next collision node (for the collision chain) */
};

/**
 * @brief Opaque type definition for the hash map structure
 * @details Simplified implementation: currently there is no Map interface layer. The actual
 * implementation is hidden in the .c file.
 */
typedef struct cobalt_hashmap cobalt_hashmap_t;

/**
 * @brief Create a new hash map
 * @param initial_buckets Initial number of buckets. If 0, a default size will be allocated upon
 * lazy loading.
 * @return Pointer to the created hash map. Returns NULL if memory allocation fails.
 * @note The corresponding error code (such as COBALT_ERROR_OUT_OF_MEMORY) will be set internally.
 */
cobalt_hashmap_t *cobalt_hashmap_create(size_t initial_buckets);

/**
 * @brief Destroy the hash map and free memory
 * @param map Pointer to the hash map to be destroyed
 * @note Will free all nodes, key copies, and the bucket array, but will not free the user data
 * memory pointed to by value.
 */
void cobalt_hashmap_destroy(cobalt_hashmap_t *map);

/**
 * @brief Insert or update a key-value pair in the hash map
 * @param map Pointer to the hash map
 * @param key String key (an internal copy will be made and stored)
 * @param value Pointer to the value to be stored
 * @return Returns 0 on success, -1 on failure (e.g., invalid parameters or out of memory)
 * @note When the load factor (size / buckets) is greater than 0.75, rehashing (expansion) will be
 * triggered automatically.
 */
int cobalt_hashmap_put(cobalt_hashmap_t *map, const char *key, void *value);

/**
 * @brief Get the value corresponding to a key
 * @param map Pointer to the hash map
 * @param key String key
 * @return Returns the corresponding value pointer on success, or NULL if not found or an error
 * occurs.
 * @note If not found, the error code COBALT_ERROR_NOT_FOUND will be set.
 */
void *cobalt_hashmap_get(cobalt_hashmap_t *map, const char *key);

/**
 * @brief Remove a specified key-value pair from the hash map
 * @param map Pointer to the hash map
 * @param key String key to be removed
 * @return Returns 0 on successful removal, -1 if not found or parameters are invalid.
 */
int cobalt_hashmap_remove(cobalt_hashmap_t *map, const char *key);

/**
 * @brief Get the number of key-value pairs stored in the hash map
 * @param map Pointer to the hash map
 * @return The number of key-value pairs (size), or 0 if map is NULL.
 */
size_t cobalt_hashmap_size(cobalt_hashmap_t *map);

/**
 * @brief Get the current number of buckets (capacity) of the hash map
 * @param map Pointer to the hash map
 * @return The number of currently allocated buckets (used for debugging/verifying expansion logic),
 * or 0 if map is NULL.
 */
size_t cobalt_hashmap_capacity(const cobalt_hashmap_t *map);

/** @} */

#endif /* HASHMAP_H */
