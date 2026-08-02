#ifndef MAP_H
#define MAP_H

/**
 * @file map.h
 * @brief Key-value map interface
 *
 * Defines an abstract interface for storage structures based on key-value pairs (such as hash tables, tree maps, etc.).
 */

#include <stddef.h>

/**
 * @defgroup Map Map Interface Module
 * @{
 */

/**
 * @brief Map abstract type
 */
typedef struct cobalt_map cobalt_map_t;

/**
 * @brief Map structure definition (interface polymorphism)
 * @details Provides standard map operations such as get, insert, remove, and size query
 */
struct cobalt_map {
    /**
     * @brief Get the corresponding value by key
     * @param self Map instance pointer
     * @param key The key to look up
     * @return A pointer to the corresponding value if found, NULL otherwise
     */
    void *(*get)(cobalt_map_t *self, const void *key);

    /**
     * @brief Insert or update a key-value pair
     * @param self Map instance pointer
     * @param key The key to insert
     * @param value The value to associate
     * @return 0 on success, non-zero error code on failure
     */
    int (*put)(cobalt_map_t *self, const void *key, void *value);

    /**
     * @brief Remove an element by key
     * @param self Map instance pointer
     * @param key The key to remove
     * @return 0 on successful removal, non-zero error code if not found or on failure
     */
    int (*remove)(cobalt_map_t *self, const void *key);

    /**
     * @brief Get the number of key-value pairs stored in the map
     * @param self Map instance pointer
     * @return The number of elements in the map
     */
    size_t (*size)(cobalt_map_t *self);

    /**
     * @brief Check if the map is empty
     * @param self Map instance pointer
     * @return Non-zero (1) if the map is empty, 0 otherwise
     */
    int (*is_empty)(cobalt_map_t *self);
};

/**
 * @brief Create a map instance
 * @return Returns a pointer to the newly created map on success, or NULL on failure
 * @note Memory needs to be freed using cobalt_map_destroy()
 */
cobalt_map_t *cobalt_map_create(void);

/**
 * @brief Destroy a map instance
 * @param map Pointer to the map to be destroyed
 */
void cobalt_map_destroy(cobalt_map_t *map);

/** @} */

#endif /* MAP_H */
