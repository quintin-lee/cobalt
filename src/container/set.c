/**
 * @file set.c
 * @brief Set container implementation
 * @details Set data structure implemented by wrapping cobalt_hashmap.
 * Note: Because the string key processing logic of the hash map is used underlyingly, the item here is actually treated as a C string (const char*).
 */

#include "cobalt/container/set.h"
#include "cobalt/container/hashmap.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

/**
 * @brief Internal set structure
 */
struct cobalt_set {
    cobalt_hashmap_t *map; /* Uses a hash map to store set elements, elements serve as both key and value */
};

/**
 * @brief Create a set
 * @details Internally calls the hash map creation logic.
 * 
 * @param initial_capacity Initial capacity
 * @return Newly allocated set pointer, NULL if out of memory
 */
cobalt_set_t *cobalt_set_create(size_t initial_capacity)
{
    cobalt_set_t *set = malloc(sizeof(cobalt_set_t));
    if (!set) {
        return NULL;
    }

    set->map = cobalt_hashmap_create(initial_capacity);
    if (!set->map) {
        free(set);
        return NULL;
    }

    return set;
}

/**
 * @brief Destroy the set
 * @details Frees the underlying hash map and the set structure itself.
 * 
 * @param set Pointer to the set
 */
void cobalt_set_destroy(cobalt_set_t *set)
{
    if (set) {
        cobalt_hashmap_destroy(set->map);
        free(set);
    }
}

/**
 * @brief Insert an element into the set
 * @details Stores the item as both the key and value of the hash map.
 * 
 * @param set Pointer to the set
 * @param item Element (treated as a string)
 * @return Returns 0 on success, -1 on failure
 */
int cobalt_set_insert(cobalt_set_t *set, void *item)
{
    if (!set) {
        return -1;
    }
    // Underlying hashmap_put accepts const char* key
    return cobalt_hashmap_put(set->map, (const char *)item, item) == 0 ? 0 : -1;
}

/**
 * @brief Remove an element from the set
 * 
 * @param set Pointer to the set
 * @param item Element (treated as a string)
 * @return Returns 0 on success, -1 on failure
 */
int cobalt_set_remove(cobalt_set_t *set, void *item)
{
    if (!set || !item) {
        return -1;
    }
    return cobalt_hashmap_remove(set->map, (const char *)item) == 0 ? 0 : -1;
}

/**
 * @brief Determine if an element is in the set
 * 
 * @param set Pointer to the set
 * @param item Element to look for
 * @return Returns 1 if present, 0 if not present or on parameter error
 */
int cobalt_set_contains(cobalt_set_t *set, void *item)
{
    if (!set || !item) {
        return 0;
    }
    return cobalt_hashmap_get(set->map, (const char *)item) != NULL;
}

/**
 * @brief Get the set size
 * 
 * @param set Pointer to the set
 * @return Number of elements
 */
size_t cobalt_set_size(cobalt_set_t *set)
{
    if (!set) {
        return 0;
    }
    return cobalt_hashmap_size(set->map);
}

/**
 * @brief Determine if the set is empty
 * 
 * @param set Pointer to the set
 * @return Returns 1 if empty, 0 if not empty
 */
int cobalt_set_is_empty(cobalt_set_t *set)
{
    return set ? cobalt_hashmap_size(set->map) == 0 : 1;
}
