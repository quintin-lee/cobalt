#ifndef MAP_H
#define MAP_H

/**
 * @file map.h
 * @brief Map (key-value) abstract interface
 *
 * Defines a generic key-value mapping interface. HashMap and TreeMap
 * embed this as their first member to support polymorphic map operations.
 */

#include <stddef.h>

/**
 * @defgroup Map Map Interface
 * @{
 */

/**
 * @brief Forward declaration of the map type
 */
typedef struct cobalt_map cobalt_map_t;

/**
 * @brief Forward declaration of the map iterator type
 */
typedef struct cobalt_map_iterator cobalt_map_iterator_t;

/**
 * @brief A key-value pair yielded by map iteration
 */
typedef struct cobalt_map_pair {
    const void *key;
    void       *value;
} cobalt_map_pair_t;

/**
 * @brief Map interface structure (embedded as first member of concrete types)
 * @details Use function pointers for polymorphic map operations.
 *          Do not instantiate directly.
 */
struct cobalt_map {
    void *(*get)(cobalt_map_t *self, const void *key, size_t key_len);
    int (*put)(cobalt_map_t *self, const void *key, size_t key_len, void *value);
    int (*remove)(cobalt_map_t *self, const void *key, size_t key_len);
    int (*contains)(cobalt_map_t *self, const void *key, size_t key_len);
    void (*clear)(cobalt_map_t *self);
    size_t (*size)(cobalt_map_t *self);
    int (*is_empty)(cobalt_map_t *self);
    cobalt_map_iterator_t *(*iterator)(cobalt_map_t *self);
    void (*destroy)(cobalt_map_t *self);
};

/* -------------------------------------------------------------------------- */
/* Map Iterator                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Map iterator virtual function table
 */
typedef struct cobalt_map_iterator_vtable {
    int (*has_next)(void *ctx);
    cobalt_map_pair_t (*next)(void *ctx);
    void (*destroy)(void *ctx);
} cobalt_map_iterator_vtable_t;

/**
 * @brief Map iterator structure
 */
struct cobalt_map_iterator {
    const cobalt_map_iterator_vtable_t *vtable;
    void                               *data;
};

/* -------------------------------------------------------------------------- */
/* Map Iterator API                                                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create a map iterator
 * @param map Map instance
 * @return Iterator pointer, or NULL on failure
 */
cobalt_map_iterator_t *cobalt_map_iterator_create(cobalt_map_t *map);

/**
 * @brief Check if there is a next element
 */
int cobalt_map_iterator_has_next(cobalt_map_iterator_t *iter);

/**
 * @brief Get the next key-value pair
 * @details Returns {NULL, NULL} when exhausted.
 */
cobalt_map_pair_t cobalt_map_iterator_next(cobalt_map_iterator_t *iter);

/**
 * @brief Destroy the iterator
 */
void cobalt_map_iterator_destroy(cobalt_map_iterator_t *iter);

/* -------------------------------------------------------------------------- */
/* Convenience API                                                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Get a value from a map
 * @param map Map instance
 * @param key Key to look up
 * @param key_len Length of the key in bytes
 * @return Value pointer, or NULL if not found
 */
void *cobalt_map_get(cobalt_map_t *map, const void *key, size_t key_len);

/**
 * @brief Insert or update a key-value pair in a map
 * @param map Map instance
 * @param key Key to insert
 * @param key_len Length of the key in bytes
 * @param value Value to store
 * @return 0 on success, -1 on failure
 */
int cobalt_map_put(cobalt_map_t *map, const void *key, size_t key_len, void *value);

/**
 * @brief Remove a key-value pair from a map
 * @param map Map instance
 * @param key Key to remove
 * @param key_len Length of the key in bytes
 * @return 0 on success, -1 if not found
 */
int cobalt_map_remove(cobalt_map_t *map, const void *key, size_t key_len);

/**
 * @brief Check if a key exists in the map
 * @param map Map instance
 * @param key Key to check
 * @param key_len Length of the key in bytes
 * @return Non-zero if the key exists, 0 otherwise
 */
int cobalt_map_contains(cobalt_map_t *map, const void *key, size_t key_len);

/**
 * @brief Remove all entries from a map
 * @param map Map instance
 */
void cobalt_map_clear(cobalt_map_t *map);

/**
 * @brief Get the number of entries in a map
 * @param map Map instance
 * @return Number of entries, or 0 if map is NULL
 */
size_t cobalt_map_size(cobalt_map_t *map);

/**
 * @brief Check if a map is empty
 * @param map Map instance
 * @return Non-zero if empty, 0 otherwise
 */
int cobalt_map_is_empty(cobalt_map_t *map);

/** @} */

#endif /* MAP_H */
