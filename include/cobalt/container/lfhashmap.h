#ifndef COBALT_CONTAINER_LFHASHMAP_H
#define COBALT_CONTAINER_LFHASHMAP_H

/**
 * @file lfhashmap.h
 * @brief Lock-Free Concurrent Extensible Hashmap with Epoch-Based Reclamation
 * @details Implements a 100% lock-free concurrent hashmap using the Split-Ordered
 *          List algorithm (Shalev & Shavit, 2006). Features wait-free point reads,
 *          zero-copy dynamic bucket expansion via a two-level segmented directory,
 *          Harris marked-pointer two-phase deletion, and integration with Layer 7
 *          cobalt_ebr_t for safe deferred memory reclamation.
 *
 * @defgroup LFHASHMAP Lock-Free Hashmap
 * @ingroup Containers
 * @{
 */

#include "cobalt/memory/allocator.h"
#include "cobalt/memory/ebr.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COBALT_LFHASHMAP_MARK_BIT ((uintptr_t)0x1)

#ifndef COBALT_IS_MARKED
#define COBALT_IS_MARKED(ptr) (((uintptr_t)(ptr) & (uintptr_t)0x1) != 0)
#endif
#ifndef COBALT_GET_UNMARKED
#define COBALT_GET_UNMARKED(ptr) ((void *)((uintptr_t)(ptr) & ~(uintptr_t)0x1))
#endif
#ifndef COBALT_GET_MARKED
#define COBALT_GET_MARKED(ptr) ((void *)((uintptr_t)(ptr) | (uintptr_t)0x1))
#endif

/**
 * @brief Opaque structure representing the lock-free hashmap
 */
typedef struct cobalt_lfhashmap cobalt_lfhashmap_t;

/**
 * @brief Opaque structure representing a concurrent safe iterator
 */
typedef struct cobalt_lfhashmap_iter cobalt_lfhashmap_iter_t;

/**
 * @brief Explicit EBR guard structure for batch operations
 */
typedef struct cobalt_lfhashmap_guard {
    cobalt_ebr_handle_t *handle;
} cobalt_lfhashmap_guard_t;

/**
 * @brief Hash function signature for generic keys
 */
typedef uint64_t (*cobalt_lfhash_func_t)(const void *key);

/**
 * @brief Key equality function signature
 */
typedef int (*cobalt_lfequal_func_t)(const void *a, const void *b);

/**
 * @brief Create a lock-free hashmap for generic keys (default string FNV-1a if NULL)
 * @param hash_fn Hash function callback (or NULL for default string hash)
 * @param equal_fn Key equality comparison callback (or NULL for default strcmp)
 * @return Created hashmap handle or NULL on allocation failure
 */
cobalt_lfhashmap_t *cobalt_lfhashmap_create(cobalt_lfhash_func_t  hash_fn,
                                            cobalt_lfequal_func_t equal_fn);

/**
 * @brief Create a lock-free hashmap specialized for integer and pointer keys
 * @note Uses inline SplitMix64 hash and direct pointer comparison (a == b) with zero function
 * pointer overhead
 * @return Created hashmap handle or NULL on allocation failure
 */
cobalt_lfhashmap_t *cobalt_lfhashmap_create_ptr(void);

/**
 * @brief Create a lock-free hashmap with custom allocator and optional shared EBR
 * @param hash_fn Hash function callback
 * @param equal_fn Equality comparison callback
 * @param alloc Custom memory allocator (or NULL for system allocator)
 * @param ebr Shared EBR engine (or NULL to create internal instance)
 * @param fast_ptr_mode True to use direct pointer equality and inline integer hash
 * @return Created hashmap handle or NULL on allocation failure
 */
cobalt_lfhashmap_t *cobalt_lfhashmap_create_with_allocator(cobalt_lfhash_func_t  hash_fn,
                                                           cobalt_lfequal_func_t equal_fn,
                                                           cobalt_allocator_t   *alloc,
                                                           cobalt_ebr_t         *ebr,
                                                           bool                  fast_ptr_mode);

/**
 * @brief Destroy a lock-free hashmap and release all internal memory
 * @param map Hashmap handle
 */
void cobalt_lfhashmap_destroy(cobalt_lfhashmap_t *map);

/**
 * @brief Insert a key-value pair into the hashmap
 * @param map Hashmap handle
 * @param key Key pointer
 * @param value Value pointer
 * @return 0 on success, COBALT_ERROR_ALREADY_EXISTS if key exists, negative on error
 */
int cobalt_lfhashmap_insert(cobalt_lfhashmap_t *map, void *key, void *value);

/**
 * @brief Insert or update a key-value pair in the hashmap (Upsert)
 * @param map Hashmap handle
 * @param key Key pointer
 * @param value Value pointer
 * @param out_old_value Optional pointer to receive previously stored value if key existed
 * @return 0 on success, negative on error
 */
int cobalt_lfhashmap_put(cobalt_lfhashmap_t *map, void *key, void *value, void **out_old_value);

/**
 * @brief Retrieve value associated with key (Wait-Free read)
 * @param map Hashmap handle
 * @param key Key pointer
 * @param out_value Optional pointer to receive value
 * @return 0 on success, COBALT_ERROR_NOT_FOUND if not present, negative on error
 */
int cobalt_lfhashmap_get(cobalt_lfhashmap_t *map, const void *key, void **out_value);

/**
 * @brief Check if key is present in the hashmap
 * @param map Hashmap handle
 * @param key Key pointer
 * @return true if key exists and is unmarked, false otherwise
 */
bool cobalt_lfhashmap_contains(cobalt_lfhashmap_t *map, const void *key);

/**
 * @brief Remove a key from the hashmap using Harris marked pointer two-phase deletion
 * @param map Hashmap handle
 * @param key Key pointer
 * @param out_old_value Optional pointer to receive removed value
 * @return 0 on success, COBALT_ERROR_NOT_FOUND if not found, negative on error
 */
int cobalt_lfhashmap_remove(cobalt_lfhashmap_t *map, const void *key, void **out_old_value);

/**
 * @brief Get total number of active elements in the hashmap
 * @param map Hashmap handle
 * @return Active element count
 */
size_t cobalt_lfhashmap_size(const cobalt_lfhashmap_t *map);

/**
 * @brief Get current logical bucket capacity (power of 2)
 * @param map Hashmap handle
 * @return Bucket count
 */
size_t cobalt_lfhashmap_bucket_count(const cobalt_lfhashmap_t *map);

/**
 * @brief Check if hashmap is empty
 * @param map Hashmap handle
 * @return true if empty, false otherwise
 */
bool cobalt_lfhashmap_is_empty(const cobalt_lfhashmap_t *map);

/**
 * @brief Explicitly pin the calling thread's EBR epoch for batch operations
 * @param map Hashmap handle
 * @return Guard holding EBR thread handle
 */
cobalt_lfhashmap_guard_t cobalt_lfhashmap_pin(cobalt_lfhashmap_t *map);

/**
 * @brief Unpin EBR epoch previously acquired by cobalt_lfhashmap_pin
 * @param guard Pointer to guard
 */
void cobalt_lfhashmap_unpin(cobalt_lfhashmap_guard_t *guard);

/**
 * @brief Create a concurrent iterator traversing all active elements
 * @param map Hashmap handle
 * @return Iterator handle or NULL on failure
 */
cobalt_lfhashmap_iter_t *cobalt_lfhashmap_iter_create(cobalt_lfhashmap_t *map);

/**
 * @brief Advance iterator to the next active regular element (skips dummy sentinels and marked
 * nodes)
 * @param iter Iterator handle
 * @param out_key Optional pointer to receive current key
 * @param out_value Optional pointer to receive current value
 * @return true if an element was retrieved, false if iteration complete
 */
bool cobalt_lfhashmap_iter_next(cobalt_lfhashmap_iter_t *iter, void **out_key, void **out_value);

/**
 * @brief Destroy iterator and release pinned epoch
 * @param iter Iterator handle
 */
void cobalt_lfhashmap_iter_destroy(cobalt_lfhashmap_iter_t *iter);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* COBALT_CONTAINER_LFHASHMAP_H */
