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
    int  (*put)(cobalt_map_t *self, const void *key, size_t key_len, void *value);
    int  (*remove)(cobalt_map_t *self, const void *key, size_t key_len);
    size_t (*size)(cobalt_map_t *self);
    int  (*is_empty)(cobalt_map_t *self);
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
    void *data;
};

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

/** @} */

#endif /* MAP_H */
