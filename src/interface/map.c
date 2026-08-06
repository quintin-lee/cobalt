/**
 * @file map.c
 * @brief Map interface implementation
 */
#include "cobalt/interface/map.h"
#include <stdlib.h>

/* -------------------------------------------------------------------------- */
/* Map iterator public API                                                    */
/* -------------------------------------------------------------------------- */

/// @brief Create a map iterator
/// @param map Map instance
/// @return Iterator pointer, or NULL on failure

cobalt_map_iterator_t *cobalt_map_iterator_create(cobalt_map_t *map)
{
    if (!map || !map->iterator) {
        return NULL;
    }
    return map->iterator(map);
}

/// @brief Check if there is a next element
/// @param iter Iterator pointer
/// @return Non-zero if there is a next element

int cobalt_map_iterator_has_next(cobalt_map_iterator_t *iter)
{
    if (!iter || !iter->vtable) {
        return 0;
    }
    return iter->vtable->has_next(iter->data);
}

/// @brief Get the next key-value pair
/// @param iter Iterator pointer
/// @return Key-value pair; {NULL, NULL} when exhausted

cobalt_map_pair_t cobalt_map_iterator_next(cobalt_map_iterator_t *iter)
{
    cobalt_map_pair_t empty = {NULL, NULL};
    if (!iter || !iter->vtable) {
        return empty;
    }
    return iter->vtable->next(iter->data);
}

/// @brief Destroy the map iterator
/// @param iter Iterator pointer

void cobalt_map_iterator_destroy(cobalt_map_iterator_t *iter)
{
    if (!iter) {
        return;
    }
    if (iter->vtable && iter->vtable->destroy) {
        iter->vtable->destroy(iter->data);
    }
    free(iter);
}

/* -------------------------------------------------------------------------- */
/* Convenience API                                                            */
/* -------------------------------------------------------------------------- */

/// @brief Get a value from a map (convenience wrapper)
/// @param map Map instance
/// @param key Key to look up
/// @param key_len Length of the key in bytes
/// @return Value pointer, or NULL if not found

void *cobalt_map_get(cobalt_map_t *map, const void *key, size_t key_len)
{
    if (!map || !map->get) {
        return NULL;
    }
    return map->get(map, key, key_len);
}

/// @brief Insert or update a key-value pair (convenience wrapper)
/// @param map Map instance
/// @param key Key to insert
/// @param key_len Length of the key in bytes
/// @param value Value to store
/// @return 0 on success, -1 on failure

int cobalt_map_put(cobalt_map_t *map, const void *key, size_t key_len, void *value)
{
    if (!map || !map->put) {
        return -1;
    }
    return map->put(map, key, key_len, value);
}

/// @brief Remove a key-value pair (convenience wrapper)
/// @param map Map instance
/// @param key Key to remove
/// @param key_len Length of the key in bytes
/// @return 0 on success, -1 if not found

int cobalt_map_remove(cobalt_map_t *map, const void *key, size_t key_len)
{
    if (!map || !map->remove) {
        return -1;
    }
    return map->remove(map, key, key_len);
}

/// @brief Check if a key exists in a map (convenience wrapper)
/// @param map Map instance
/// @param key Key to check
/// @param key_len Length of the key in bytes
/// @return Non-zero if the key exists, 0 otherwise

int cobalt_map_contains(cobalt_map_t *map, const void *key, size_t key_len)
{
    if (!map || !map->contains) {
        return 0;
    }
    return map->contains(map, key, key_len);
}

/// @brief Remove all entries from a map (convenience wrapper)
/// @param map Map instance

void cobalt_map_clear(cobalt_map_t *map)
{
    if (!map || !map->clear) {
        return;
    }
    map->clear(map);
}

/// @brief Get the number of entries (convenience wrapper)
/// @param map Map instance
/// @return Number of entries, or 0 if map is NULL

size_t cobalt_map_size(cobalt_map_t *map)
{
    if (!map || !map->size) {
        return 0;
    }
    return map->size(map);
}

/// @brief Check if a map is empty (convenience wrapper)
/// @param map Map instance
/// @return Non-zero if empty, 0 otherwise

int cobalt_map_is_empty(cobalt_map_t *map)
{
    if (!map || !map->is_empty) {
        return 1;
    }
    return map->is_empty(map);
}
