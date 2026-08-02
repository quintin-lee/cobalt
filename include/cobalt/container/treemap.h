#ifndef TREEMAP_H
#define TREEMAP_H

/**
 * @file treemap.h
 * @brief Ordered map based on tree structure (simplified implementation, currently an ordinary
 * binary search tree)
 * @details Provides a key-value pair container sorted by key value.
 */

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
void *cobalt_treemap_get(cobalt_treemap_t *map, const char *key);

/**
 * @brief Remove a specified key-value pair
 * @param map Pointer to the map
 * @param key String key
 * @return Returns 0 on successful removal, -1 if not found or on failure
 * @note The simplified implementation currently only sets the value to NULL, and does not actually
 * delete the node or rebalance the tree.
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
size_t cobalt_treemap_size(cobalt_treemap_t *map);

/** @} */

#endif /* TREEMAP_H */
