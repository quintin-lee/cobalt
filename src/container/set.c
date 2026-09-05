/**
 * @file set.c
 * @brief Set container implementation
 * @details Set data structure implemented by wrapping cobalt_hashmap.
 *          Embeds cobalt_map_t as first member for polymorphic map interface.
 */

#include "cobalt/container/set.h"
#include "cobalt/container/hashmap.h"
#include "cobalt/interface/map.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/runtime/error.h"
#include "cobalt/utils/string.h"
#include <stdlib.h>

/* Sentinel value used as map value to indicate set membership. */
static const int set_sentinel = 1;

/* -------------------------------------------------------------------------- */
/* Opaque type definition (must be before any function that dereferences it)  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Opaque set structure
 * @details Backed by a hashmap holding a sentinel value per member.
 *          Embeds cobalt_map_t as first member for map polymorphism.
 */
struct cobalt_set {
    cobalt_map_t        base;
    cobalt_hashmap_t   *map;
    cobalt_allocator_t *alloc;
};

/* -------------------------------------------------------------------------- */
/* Map vtable (set-specific)                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief look up membership sentinel through map interface slot
 * @param self map interface pointer
 * @param key element pointer
 * @param key_len element length
 * @return sentinel pointer when member, NULL otherwise
 */
static void *set_map_get(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    if (cobalt_hashmap_get_ext(set->map, key, key_len) != NULL) {
        return (void *)&set_sentinel;
    }
    return NULL;
}

/**
 * @brief insert element through map interface slot
 * @param self map interface pointer
 * @param key element pointer
 * @param key_len element length
 * @param value unused value slot
 * @return 0 on success, -1 on failure
 */
static int set_map_put(cobalt_map_t *self, const void *key, size_t key_len, void *value)
{
    (void)value;
    cobalt_set_t *set = (cobalt_set_t *)self;
    int           ret = cobalt_hashmap_put_ext(set->map, key, key_len, (void *)&set_sentinel);
    return (ret == 0) ? 0 : -1;
}

/**
 * @brief remove element through map interface slot
 * @param self map interface pointer
 * @param key element pointer
 * @param key_len element length
 * @return 0 on success, -1 when element absent
 */
static int set_map_remove(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    return cobalt_hashmap_remove_ext(set->map, key, key_len);
}

/**
 * @brief test membership through map interface slot
 * @param self map interface pointer
 * @param key element pointer
 * @param key_len element length
 * @return nonzero when element is a member
 */
static int set_map_contains(cobalt_map_t *self, const void *key, size_t key_len)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    return cobalt_hashmap_get_ext(set->map, key, key_len) != NULL;
}

/**
 * @brief drop all elements through map interface slot
 * @param self map interface pointer
 * @note rebuilds backing map, reports allocation failure via error stack
 */
static void set_map_clear(cobalt_map_t *self)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    cobalt_hashmap_destroy(set->map);
    set->map = cobalt_hashmap_create_with_allocator(0, set->alloc);
    if (!set->map) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
    }
}

/**
 * @brief report member count through map interface slot
 * @param self map interface pointer
 * @return number of members
 */
static size_t set_map_size(cobalt_map_t *self)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    return cobalt_hashmap_size(set->map);
}

/**
 * @brief report emptiness through map interface slot
 * @param self map interface pointer
 * @return nonzero when set holds no members
 */
static int set_map_is_empty(cobalt_map_t *self)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    return cobalt_hashmap_size(set->map) == 0;
}

/**
 * @brief create member iterator through map interface slot
 * @param self map interface pointer
 * @return new iterator, or NULL on allocation failure
 */
static cobalt_map_iterator_t *set_map_iterator(cobalt_map_t *self)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    return cobalt_hashmap_iterator_create(set->map);
}

/**
 * @brief destroy set through map interface slot
 * @param self map interface pointer
 */
static void set_map_destroy(cobalt_map_t *self)
{
    cobalt_set_t *set = (cobalt_set_t *)self;
    cobalt_set_destroy(set);
}

static const cobalt_map_t set_map_vtable = {
    .get      = set_map_get,
    .put      = set_map_put,
    .remove   = set_map_remove,
    .contains = set_map_contains,
    .clear    = set_map_clear,
    .size     = set_map_size,
    .is_empty = set_map_is_empty,
    .iterator = set_map_iterator,
    .destroy  = set_map_destroy,
};

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief create empty set using system allocator
 * @param initial_capacity backing map bucket hint
 * @return new set, or NULL on allocation failure
 */
cobalt_set_t *cobalt_set_create(size_t initial_capacity)
{
    return cobalt_set_create_with_allocator(initial_capacity, cobalt_allocator_get_system());
}

/**
 * @brief create empty set using given allocator
 * @param initial_capacity backing map bucket hint
 * @param alloc allocator backing set and map
 * @return new set, or NULL on allocation failure
 */
cobalt_set_t *cobalt_set_create_with_allocator(size_t initial_capacity, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_set_t *set = (cobalt_set_t *)alloc->alloc(alloc, sizeof(cobalt_set_t));
    if (!set) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    set->base = set_map_vtable;
    set->map  = cobalt_hashmap_create_with_allocator(initial_capacity, alloc);
    if (!set->map) {
        alloc->free(alloc, set);
        return NULL;
    }
    set->alloc = alloc;
    return set;
}

/**
 * @brief create empty set with custom hash and equality functions
 * @param initial_capacity backing map bucket hint
 * @param hash_func element hash function
 * @param equal_func element equality function
 * @return new set, or NULL on allocation failure
 */
cobalt_set_t *cobalt_set_create_ext(size_t                  initial_capacity,
                                    cobalt_set_hash_func_t  hash_func,
                                    cobalt_set_equal_func_t equal_func)
{
    return cobalt_set_create_ext_with_allocator(
        initial_capacity, hash_func, equal_func, cobalt_allocator_get_system());
}

/**
 * @brief create empty set with custom callbacks and allocator
 * @param initial_capacity backing map bucket hint
 * @param hash_func element hash function
 * @param equal_func element equality function
 * @param alloc allocator backing set and map
 * @return new set, or NULL on allocation failure
 */
cobalt_set_t *cobalt_set_create_ext_with_allocator(size_t                  initial_capacity,
                                                   cobalt_set_hash_func_t  hash_func,
                                                   cobalt_set_equal_func_t equal_func,
                                                   cobalt_allocator_t     *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_set_t *set = (cobalt_set_t *)alloc->alloc(alloc, sizeof(cobalt_set_t));
    if (!set) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    set->base = set_map_vtable;
    set->map  = cobalt_hashmap_create_ext(
        initial_capacity, (cobalt_hash_func_t)hash_func, (cobalt_equal_func_t)equal_func);
    if (!set->map) {
        alloc->free(alloc, set);
        return NULL;
    }
    set->alloc = alloc;
    return set;
}

/**
 * @brief destroy set and release backing map
 * @param set set to destroy, NULL is no-op
 */
void cobalt_set_destroy(cobalt_set_t *set)
{
    if (set) {
        cobalt_hashmap_destroy(set->map);
        set->alloc->free(set->alloc, set);
    }
}

/**
 * @brief insert string element into set
 * @param set set instance
 * @param item string element to insert
 * @return 0 on success, -1 on failure
 */
int cobalt_set_insert(cobalt_set_t *set, void *item)
{
    if (!set || !item) {
        return -1;
    }
    return cobalt_hashmap_put(set->map, (const char *)item, item) == 0 ? 0 : -1;
}

/**
 * @brief insert binary element of given length into set
 * @param set set instance
 * @param item element bytes to insert
 * @param item_len element length in bytes
 * @return 0 on success, -1 on failure
 */
int cobalt_set_insert_ext(cobalt_set_t *set, const void *item, size_t item_len)
{
    if (!set || !item) {
        return -1;
    }
    return cobalt_hashmap_put_ext(set->map, item, item_len, (void *)item) == 0 ? 0 : -1;
}

/**
 * @brief remove string element from set
 * @param set set instance
 * @param item string element to remove
 * @return 0 on success, -1 when element absent
 */
int cobalt_set_remove(cobalt_set_t *set, void *item)
{
    if (!set || !item) {
        return -1;
    }
    return cobalt_hashmap_remove(set->map, (const char *)item) == 0 ? 0 : -1;
}

/**
 * @brief remove binary element of given length from set
 * @param set set instance
 * @param item element bytes to remove
 * @param item_len element length in bytes
 * @return 0 on success, -1 when element absent
 */
int cobalt_set_remove_ext(cobalt_set_t *set, const void *item, size_t item_len)
{
    if (!set || !item) {
        return -1;
    }
    return cobalt_hashmap_remove_ext(set->map, item, item_len) == 0 ? 0 : -1;
}

/**
 * @brief test whether string element is a member
 * @param set set instance
 * @param item string element to test
 * @return nonzero when member, zero otherwise
 */
int cobalt_set_contains(cobalt_set_t *set, void *item)
{
    if (!set || !item) {
        return 0;
    }
    return cobalt_hashmap_get(set->map, (const char *)item) != NULL;
}

/**
 * @brief test whether binary element of given length is a member
 * @param set set instance
 * @param item element bytes to test
 * @param item_len element length in bytes
 * @return nonzero when member, zero otherwise
 */
int cobalt_set_contains_ext(cobalt_set_t *set, const void *item, size_t item_len)
{
    if (!set || !item) {
        return 0;
    }
    return cobalt_hashmap_get_ext(set->map, item, item_len) != NULL;
}

/**
 * @brief report number of members in set
 * @param set set instance
 * @return member count, zero for NULL set
 */
size_t cobalt_set_size(const cobalt_set_t *set)
{
    return set ? cobalt_hashmap_size(set->map) : 0;
}

/**
 * @brief report whether set holds no members
 * @param set set instance
 * @return nonzero when empty, zero otherwise
 */
int cobalt_set_is_empty(const cobalt_set_t *set)
{
    return set ? cobalt_hashmap_size(set->map) == 0 : 1;
}

/// @brief Get the internal sentinel pointer for an element (via map interface)
/// @param set Set instance
/// @param item Element to look up
/// @return Non-NULL pointer if element exists (internal sentinel), NULL if not found
/// @note Prefer cobalt_set_contains() for existence checks.
void *cobalt_set_get(const cobalt_set_t *set, void *item)
{
    if (!set || !item) {
        return NULL;
    }
    return cobalt_hashmap_get(set->map, (const char *)item);
}

/* -------------------------------------------------------------------------- */
/* Public iterator factory                                                    */
/* -------------------------------------------------------------------------- */

/// @brief Create a map iterator for this set
/// @param set Set instance
/// @return Iterator pointer, or NULL on failure
/// @note Returns a cobalt_map_iterator_t compatible with the Map interface.
///       The value field of each yielded pair is the internal sentinel.
///       Destroy with cobalt_map_iterator_destroy().
cobalt_map_iterator_t *cobalt_set_iterator_create(cobalt_set_t *set)
{
    if (!set) {
        return NULL;
    }
    return cobalt_hashmap_iterator_create(set->map);
}
