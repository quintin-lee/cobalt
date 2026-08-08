#ifndef TREEMAP_H
#define TREEMAP_H

/**
 * @file treemap.h
 * @brief Ordered map based on tree structure (true Red-Black tree implementation)
 * @details Provides a key-value pair container sorted by key value.
 */

#include "cobalt/interface/map.h"
#include "cobalt/memory/allocator.h"
#include <stddef.h>

/**
 * @defgroup treemap Tree Map (TreeMap)
 * @brief Ordered key-value pair mapping implementation
 * @{
 */

/**
 * @brief Opaque type definition for TreeMap
 * @details The actual implementation is hidden in the .c file.
 */
typedef struct cobalt_treemap cobalt_treemap_t;

/**
 * @brief Generic comparison function type for TreeMap
 * @param a  Pointer to first key
 * @param b  Pointer to second key
 * @return Negative if a < b, zero if a == b, positive if a > b
 */
typedef int (*cobalt_compare_func_t)(const void *a, const void *b);

/**
 * @brief Create a new TreeMap (string keys, lexicographic order)
 * @return Returns the newly created map pointer on success, NULL on failure
 */
cobalt_treemap_t *cobalt_treemap_create(void);

/**
 * @brief Create a new TreeMap with a custom comparison function
 * @param compare_func Custom comparison function (may be NULL for default strcmp)
 * @return Returns the newly created map pointer on success, NULL on failure
 * @note When a custom comparator is provided, keys are NOT copied — the caller must
 *       ensure key lifetime exceeds the map's lifetime. With NULL, string keys are
 *       deep-copied as with cobalt_treemap_create().
 */
cobalt_treemap_t *cobalt_treemap_create_ext(cobalt_compare_func_t compare_func);

/**
 * @brief Destroy the TreeMap and free memory
 * @param map Pointer to the map to be destroyed
 */
void cobalt_treemap_destroy(cobalt_treemap_t *map);

/**
 * @brief Insert or update a key-value pair
 * @param map Pointer to the map
 * @param key String key
 * @param value Pointer to the value to be stored
 * @return Returns 0 on success, -1 on failure
 */
int cobalt_treemap_put(cobalt_treemap_t *map, const char *key, void *value);

/**
 * @brief Get the corresponding value based on a key
 * @param map Pointer to the map
 * @param key String key
 * @return The found value pointer, or NULL if not found
 */
void *cobalt_treemap_get(const cobalt_treemap_t *map, const char *key);

/**
 * @brief Remove a specified key-value pair
 * @param map Pointer to the map
 * @param key String key
 * @return Returns 0 on successful removal, -1 if not found or on failure
 */
int cobalt_treemap_remove(cobalt_treemap_t *map, const char *key);

/**
 * @brief Get the minimum key in the map
 * @param map Pointer to the map
 * @return The minimum string key, or NULL if empty
 */
const char *cobalt_treemap_min_key(cobalt_treemap_t *map);

/**
 * @brief Get the maximum key in the map
 * @param map Pointer to the map
 * @return The maximum string key, or NULL if empty
 */
const char *cobalt_treemap_max_key(cobalt_treemap_t *map);

/**
 * @brief Get the size of the map
 * @param map Pointer to the map
 * @return The number of key-value pairs
 */
size_t cobalt_treemap_size(const cobalt_treemap_t *map);

/** @} */

/**
 * @brief Create a map iterator for this tree map
 * @param map Tree map instance
 * @return Iterator pointer, or NULL on failure
 * @note Returns a cobalt_map_iterator_t compatible with the Map interface.
 *       Destroy with cobalt_map_iterator_destroy().

 * @note The container is **not thread-safe**. Concurrent access from
 *                multiple threads without external synchronization leads to
 *                data races and undefined behavior. Use a mutex or other
 *                synchronization primitive to protect shared containers.
 *                Reference-counted objects (@ref cobalt_object_t) use atomic
 *                operations and are thread-safe for ref-count operations.
 */
cobalt_map_iterator_t *cobalt_treemap_iterator_create(cobalt_treemap_t *map);

#endif /* TREEMAP_H */
