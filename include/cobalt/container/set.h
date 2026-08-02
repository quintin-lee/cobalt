#ifndef SET_H
#define SET_H

/**
 * @file set.h
 * @brief Set container
 * @details A set implementation based on a hash map, used to store unique elements. The current implementation casts void* to const char* as the key for the underlying hash map, making it more suitable for storing strings.
 */

#include <stddef.h>

/**
 * @defgroup set Set (Set)
 * @brief Set container guaranteeing element uniqueness
 * @{
 */

/**
 * @brief Opaque type definition for the set
 */
typedef struct cobalt_set cobalt_set_t;

/**
 * @brief Create a new set
 * @param initial_capacity Initial capacity, uses default capacity if 0
 * @return Returns set pointer on success, NULL on failure
 */
cobalt_set_t *cobalt_set_create(size_t initial_capacity);

/**
 * @brief Destroy the set and free memory
 * @param set Pointer to the set to be destroyed
 */
void          cobalt_set_destroy(cobalt_set_t *set);

/**
 * @brief Insert an element into the set
 * @param set Pointer to the set
 * @param item Element to insert (currently treated as const char* underlyingly)
 * @return Returns 0 on successful insertion or if the element already exists, -1 on failure
 */
int           cobalt_set_insert(cobalt_set_t *set, void *item);

/**
 * @brief Remove an element from the set
 * @param set Pointer to the set
 * @param item Element to remove
 * @return Returns 0 on successful removal, -1 if not found or on failure
 */
int           cobalt_set_remove(cobalt_set_t *set, void *item);

/**
 * @brief Check if the set contains a certain element
 * @param set Pointer to the set
 * @param item Element to check
 * @return Returns a non-zero value (1) if the element is contained, 0 otherwise
 */
int           cobalt_set_contains(cobalt_set_t *set, void *item);

/**
 * @brief Get the number of elements in the set
 * @param set Pointer to the set
 * @return Set size
 */
size_t        cobalt_set_size(cobalt_set_t *set);

/**
 * @brief Check if the set is empty
 * @param set Pointer to the set
 * @return Returns 1 if empty or if set is NULL, 0 otherwise
 */
int           cobalt_set_is_empty(cobalt_set_t *set);

/** @} */

#endif /* SET_H */
