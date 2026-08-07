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
 * @brief Create a new TreeMap
 * @return Returns the newly created map pointer on success, NULL on failure
 */
cobalt_treemap_t *cobalt_treemap_create(void);

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
 */
cobalt_map_iterator_t *cobalt_treemap_iterator_create(cobalt_treemap_t *map);

#endif /* TREEMAP_H */
