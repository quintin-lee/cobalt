#ifndef SET_H
#define SET_H

/**
 * @file set.h
 * @brief Set container
 * @details A set implementation based on a hash map, used to store unique elements.
 *
 * The default cobalt_set_create() stores C-string elements. Use cobalt_set_create_ext()
 * for generic element types with custom hash and equality callbacks.
 */

#include "cobalt/interface/map.h"
#include "cobalt/memory/allocator.h"
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
 * @brief Generic hash function type for set elements
 */
typedef unsigned int (*cobalt_set_hash_func_t)(const void *item, size_t item_len);

/**
 * @brief Generic equality function type for set elements
 */
typedef int (*cobalt_set_equal_func_t)(const void *a, const void *b, size_t item_len);

/**
 * @brief Create a new set (string elements)
 * @param initial_capacity Initial capacity, uses default capacity if 0
 * @return Returns set pointer on success, NULL on failure
 */
cobalt_set_t *cobalt_set_create(size_t initial_capacity);

/**
 * @brief Create a set with a custom allocator
 *
 * Same as cobalt_set_create() but uses the provided allocator.
 * @param initial_capacity Initial number of buckets.
 * @param alloc            Custom allocator (must not be NULL)
 * @return Pointer to the newly created set, or NULL on failure
 */
cobalt_set_t *cobalt_set_create_with_allocator(size_t initial_capacity, cobalt_allocator_t *alloc);

/**
 * @brief Create an extended set with a custom allocator
 *
 * Same as cobalt_set_create_ext() but uses the provided allocator.
 * @param initial_capacity Initial number of buckets.
 * @param hash_func        Custom hash function (may be NULL for default).
 * @param equal_func       Custom equality function (may be NULL for default).
 * @param alloc            Custom allocator (must not be NULL)
 * @return Pointer to the newly created set, or NULL on failure
 */
cobalt_set_t *cobalt_set_create_ext_with_allocator(size_t                  initial_capacity,
                                                   cobalt_set_hash_func_t  hash_func,
                                                   cobalt_set_equal_func_t equal_func,
                                                   cobalt_allocator_t     *alloc);

/**
 * @brief Create an extended set with custom hash and equality functions
 *
 * Creates a set that stores arbitrary (non-string) elements using
 * user-provided hash and equality callbacks.
 * @param initial_capacity Initial number of buckets.
 * @param hash_func        Custom hash function (may be NULL for default).
 * @param equal_func       Custom equality function (may be NULL for default).
 * @return Pointer to the newly created set, or NULL on failure
 * @note Elements are NOT copied — caller must ensure element lifetime exceeds the set's lifetime.
 */

cobalt_set_t *cobalt_set_create_ext(size_t                  initial_capacity,
                                    cobalt_set_hash_func_t  hash_func,
                                    cobalt_set_equal_func_t equal_func);

/**
 * @brief Destroy the set and free memory
 * @param set Pointer to the set to be destroyed
 */
void cobalt_set_destroy(cobalt_set_t *set);

/**
 * @brief Insert an element into the set (string elements)
 * @param set Pointer to the set
 * @param item Element to insert (C-string; a copy is made)
 * @return Returns 0 on successful insertion or if the element already exists, -1 on failure
 */
int cobalt_set_insert(cobalt_set_t *set, void *item);

/**
 * @brief Insert an element into the set (generic elements)
 * @param set Pointer to the set created with cobalt_set_create_ext()
 * @param item Element pointer
 * @param item_len Length of the element in bytes
 * @return Returns 0 on successful insertion or if the element already exists, -1 on failure
 * @note Element is NOT copied — caller must ensure element lifetime exceeds the set's lifetime.
 */
int cobalt_set_insert_ext(cobalt_set_t *set, const void *item, size_t item_len);

/**
 * @brief Remove an element from the set (string elements)
 * @param set Pointer to the set
 * @param item Element to remove (C-string)
 * @return Returns 0 on successful removal, -1 if not found or on failure
 */
int cobalt_set_remove(cobalt_set_t *set, void *item);

/**
 * @brief Remove an element from the set (generic elements)
 * @param set Pointer to the set created with cobalt_set_create_ext()
 * @param item Element pointer
 * @param item_len Length of the element in bytes
 * @return Returns 0 on successful removal, -1 if not found or on failure
 */
int cobalt_set_remove_ext(cobalt_set_t *set, const void *item, size_t item_len);

/**
 * @brief Check if the set contains a certain element (string elements)
 * @param set Pointer to the set
 * @param item Element to check (C-string)
 * @return Returns a non-zero value (1) if the element is contained, 0 otherwise
 */
int cobalt_set_contains(cobalt_set_t *set, void *item);

/**
 * @brief Check if the set contains a certain element (generic elements)
 * @param set Pointer to the set created with cobalt_set_create_ext()
 * @param item Element pointer
 * @param item_len Length of the element in bytes
 * @return Returns a non-zero value (1) if the element is contained, 0 otherwise
 */
int cobalt_set_contains_ext(cobalt_set_t *set, const void *item, size_t item_len);

/**
 * @brief Get the number of elements in the set
 * @param set Pointer to the set
 * @return Set size
 */
size_t cobalt_set_size(const cobalt_set_t *set);

/**
 * @brief Check if the set is empty
 * @param set Pointer to the set
 * @return Returns 1 if empty or if set is NULL, 0 otherwise
 */
int cobalt_set_is_empty(const cobalt_set_t *set);

/** @} */

/**
 * @brief Create a map iterator for this set
 * @param set Set instance
 * @return Iterator pointer, or NULL on failure
 * @note Returns a cobalt_map_iterator_t compatible with the Map interface.
 *       The value field of each yielded pair points to a static sentinel;
 *       the key field contains the set element.
 *       Destroy with cobalt_map_iterator_destroy().
 */
cobalt_map_iterator_t *cobalt_set_iterator_create(cobalt_set_t *set);

/**
 * @brief Get the value associated with an element (via map interface sentinel)
 * @param set Set instance
 * @param item Element to look up
 * @return Non-NULL pointer if element exists (returns internal sentinel), NULL if not found
 * @note This function exists for API symmetry with HashMap/TreeMap.
 *       Use cobalt_set_contains() to check existence without the sentinel.

 * @note The container is **not thread-safe**. Concurrent access from
 *                multiple threads without external synchronization leads to
 *                data races and undefined behavior. Use a mutex or other
 *                synchronization primitive to protect shared containers.
 *                Reference-counted objects (@ref cobalt_object_t) use atomic
 *                operations and are thread-safe for ref-count operations.
 */
void *cobalt_set_get(const cobalt_set_t *set, void *item);

#endif /* SET_H */
