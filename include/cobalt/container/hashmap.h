#ifndef HASHMAP_H
#define HASHMAP_H

/**
 * @file hashmap.h
 * @brief Hash map (dictionary) container
 * @details Provides a hash map implementation based on string keys (C-strings), using separate
 * chaining for hash collision resolution. An extended variant accepts custom hash and equality
 * callbacks for generic key types.
 */

#include <stddef.h>

/**
 * @defgroup hashmap Hash Map (HashMap)
 * @brief Hash map implementation mapping key-value pairs
 * @{
 */

/**
 * @brief Opaque type definition for the hash map node
 */
typedef struct cobalt_hashmap_node cobalt_hashmap_node_t;

/**
 * @brief Opaque type definition for the hash map structure
 */
typedef struct cobalt_hashmap cobalt_hashmap_t;

/**
 * @brief Generic hash function type
 * @param key Opaque key pointer
 * @param key_len Length of the key in bytes
 * @return 32-bit hash value
 */
typedef unsigned int (*cobalt_hash_func_t)(const void *key, size_t key_len);

/**
 * @brief Generic equality function type
 * @param a First key pointer
 * @param b Second key pointer
 * @param key_len Length of each key in bytes
 * @return Non-zero if keys are equal, zero otherwise
 */
typedef int (*cobalt_equal_func_t)(const void *a, const void *b, size_t key_len);

/**
 * @brief Create a new hash map (string keys)
 * @param initial_buckets Initial number of buckets. If 0, a default size will be allocated upon
 * lazy loading.
 * @return Pointer to the created hash map. Returns NULL if memory allocation fails.
 * @note The corresponding error code (such as COBALT_ERROR_OUT_OF_MEMORY) will be set internally.
 */
cobalt_hashmap_t *cobalt_hashmap_create(size_t initial_buckets);

/**
 * @brief Create a new hash map with custom hash and equality functions
 * @param initial_buckets Initial number of buckets. If 0, a default size will be allocated upon
 * lazy loading.
 * @param hash_func Custom hash function (may be NULL for default FNV-1a string hash)
 * @param equal_func Custom equality function (may be NULL for default strcmp)
 * @return Pointer to the created hash map. Returns NULL if memory allocation fails.
 * @note When custom callbacks are provided, keys are NOT copied — the caller must ensure key
 *       lifetime exceeds the map's lifetime. With NULL callbacks, string keys are deep-copied
 *       as with cobalt_hashmap_create().
 */
cobalt_hashmap_t *cobalt_hashmap_create_ext(size_t initial_buckets,
                                            cobalt_hash_func_t hash_func,
                                            cobalt_equal_func_t equal_func);

/**
 * @brief Destroy the hash map and free memory
 * @param map Pointer to the hash map to be destroyed
 * @note Will free all nodes, key copies (for string maps), and the bucket array, but will not
 * free the user data memory pointed to by value.
 */
void cobalt_hashmap_destroy(cobalt_hashmap_t *map);

/**
 * @brief Insert or update a key-value pair in the hash map (string keys)
 * @param map Pointer to the hash map
 * @param key String key (an internal copy will be made and stored)
 * @param value Pointer to the value to be stored
 * @return Returns 0 on success, -1 on failure (e.g., invalid parameters or out of memory)
 * @note When the load factor (size / buckets) is greater than 0.75, rehashing (expansion) will be
 * triggered automatically.
 */
int cobalt_hashmap_put(cobalt_hashmap_t *map, const char *key, void *value);

/**
 * @brief Insert or update a key-value pair (generic keys)
 * @param map Pointer to the hash map created with cobalt_hashmap_create_ext()
 * @param key Opaque key pointer
 * @param key_len Length of the key in bytes
 * @param value Pointer to the value to be stored
 * @return Returns 0 on success, -1 on failure
 * @note Key is NOT copied — caller must ensure key lifetime exceeds the map's lifetime.
 */
int cobalt_hashmap_put_ext(cobalt_hashmap_t *map, const void *key, size_t key_len, void *value);

/**
 * @brief Get the value corresponding to a key (string keys)
 * @param map Pointer to the hash map
 * @param key String key
 * @return Returns the corresponding value pointer on success, or NULL if not found or an error
 * occurs.
 * @note If not found, the error code COBALT_ERROR_NOT_FOUND will be set.
 */
void *cobalt_hashmap_get(const cobalt_hashmap_t *map, const char *key);

/**
 * @brief Get the value corresponding to a key (generic keys)
 * @param map Pointer to the hash map created with cobalt_hashmap_create_ext()
 * @param key Opaque key pointer
 * @param key_len Length of the key in bytes
 * @return Returns the corresponding value pointer on success, or NULL if not found.
 */
void *cobalt_hashmap_get_ext(const cobalt_hashmap_t *map, const void *key, size_t key_len);

/**
 * @brief Remove a specified key-value pair from the hash map (string keys)
 * @param map Pointer to the hash map
 * @param key String key to be removed
 * @return Returns 0 on successful removal, -1 if not found or parameters are invalid.
 */
int cobalt_hashmap_remove(cobalt_hashmap_t *map, const char *key);

/**
 * @brief Remove a specified key-value pair (generic keys)
 * @param map Pointer to the hash map created with cobalt_hashmap_create_ext()
 * @param key Opaque key pointer
 * @param key_len Length of the key in bytes
 * @return Returns 0 on successful removal, -1 if not found or parameters are invalid.
 */
int cobalt_hashmap_remove_ext(cobalt_hashmap_t *map, const void *key, size_t key_len);

/**
 * @brief Get the number of key-value pairs stored in the hash map
 * @param map Pointer to the hash map
 * @return The number of key-value pairs (size), or 0 if map is NULL.
 */
size_t cobalt_hashmap_size(const cobalt_hashmap_t *map);

/**
 * @brief Get the current number of buckets (capacity) of the hash map
 * @param map Pointer to the hash map
 * @return The number of currently allocated buckets (used for debugging/verifying expansion logic),
 * or 0 if map is NULL.
 */
size_t cobalt_hashmap_capacity(const cobalt_hashmap_t *map);

/** @} */

#endif /* HASHMAP_H */
