#ifndef COBALT_CONTAINER_LFSKIPLIST_H
#define COBALT_CONTAINER_LFSKIPLIST_H

/**
 * @file lfskiplist.h
 * @brief Lock-Free Concurrent SkipList with Epoch-Based Reclamation
 * @details Implements a pure lock-free concurrent skip list using the Fraser /
 *          Herlihy-Lev-Shavit (HLS) algorithm with bit-0 marked pointers for two-phase
 *          deletion, cooperative physical unlinking, and wait-free point reads.
 *          Integrated with Layer 7 cobalt_ebr_t for safe deferred memory reclamation.
 *
 * @defgroup LFSkiplist Lock-Free SkipList
 * @ingroup Containers
 * @{
 */

#include "cobalt/algorithm/functional.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/memory/ebr.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COBALT_LFSKIPLIST_MAX_LEVEL 32
#define COBALT_LFSKIPLIST_MARK_BIT ((uintptr_t)0x1)

#ifndef COBALT_IS_MARKED
#define COBALT_IS_MARKED(ptr) (((uintptr_t)(ptr) & COBALT_LFSKIPLIST_MARK_BIT) != 0)
#endif
#ifndef COBALT_GET_UNMARKED
#define COBALT_GET_UNMARKED(ptr) ((void *)((uintptr_t)(ptr) & ~COBALT_LFSKIPLIST_MARK_BIT))
#endif
#ifndef COBALT_GET_MARKED
#define COBALT_GET_MARKED(ptr) ((void *)((uintptr_t)(ptr) | COBALT_LFSKIPLIST_MARK_BIT))
#endif

/**
 * @brief Opaque structure representing the lock-free skip list
 */
typedef struct cobalt_lfskiplist cobalt_lfskiplist_t;

/**
 * @brief Opaque structure representing an ordered range iterator
 */
typedef struct cobalt_lfskiplist_iter cobalt_lfskiplist_iter_t;

/**
 * @brief Explicit EBR guard structure for batch operations
 */
typedef struct cobalt_lfskiplist_guard {
    cobalt_ebr_handle_t *handle;
} cobalt_lfskiplist_guard_t;

/**
 * @brief Create a new lock-free skip list with system allocator and internal EBR
 * @param compar Key comparison function (returns <0, 0, >0)
 * @return Pointer to created skip list, or NULL on allocation failure
 */
cobalt_lfskiplist_t *cobalt_lfskiplist_create(compare_func_t compar);

/**
 * @brief Create a lock-free skip list with custom allocator and optional shared EBR
 * @param compar Key comparison function (must not be NULL)
 * @param alloc Custom memory allocator (pass NULL to use system allocator)
 * @param ebr Optional shared EBR engine (pass NULL to automatically create private EBR)
 * @return Pointer to created skip list, or NULL on failure
 */
cobalt_lfskiplist_t *cobalt_lfskiplist_create_with_allocator(compare_func_t      compar,
                                                             cobalt_allocator_t *alloc,
                                                             cobalt_ebr_t       *ebr);

/**
 * @brief Destroy a lock-free skip list and free all its internal nodes
 * @param skiplist Pointer to the skip list to destroy
 * @note Does not free user payload pointers (keys/values).
 */
void cobalt_lfskiplist_destroy(cobalt_lfskiplist_t *skiplist);

/**
 * @brief Insert a key-value pair into the skip list (no-op if key exists)
 * @param skiplist Pointer to the skip list
 * @param key User key pointer
 * @param value User value pointer
 * @return 0 on success, -1 if key already exists, -2 on invalid argument or OOM
 */
int cobalt_lfskiplist_insert(cobalt_lfskiplist_t *skiplist, void *key, void *value);

/**
 * @brief Insert or update a key-value pair (upsert)
 * @param skiplist Pointer to the skip list
 * @param key User key pointer
 * @param value User value pointer
 * @param out_old_value Output location for previous value if key was replaced (may be NULL)
 * @return 0 on new insertion, 1 on successful update of existing key, -1 on failure
 */
int cobalt_lfskiplist_put(cobalt_lfskiplist_t *skiplist,
                          void                *key,
                          void                *value,
                          void               **out_old_value);

/**
 * @brief Wait-free lookup of a value by key
 * @param skiplist Pointer to the skip list
 * @param key Key to search for
 * @param out_value Output location for found value pointer (may be NULL)
 * @return 0 if found, -1 if key does not exist or invalid argument
 */
int cobalt_lfskiplist_get(cobalt_lfskiplist_t *skiplist, const void *key, void **out_value);

/**
 * @brief Check if a key exists in the skip list
 * @param skiplist Pointer to the skip list
 * @param key Key to search for
 * @return true if key exists and is not logically deleted, false otherwise
 */
bool cobalt_lfskiplist_contains(cobalt_lfskiplist_t *skiplist, const void *key);

/**
 * @brief Remove a key from the skip list using two-phase deletion
 * @param skiplist Pointer to the skip list
 * @param key Key to remove
 * @param out_old_value Output location for removed value pointer (may be NULL)
 * @return 0 if removed, -1 if key not found or invalid argument
 */
int cobalt_lfskiplist_remove(cobalt_lfskiplist_t *skiplist, const void *key, void **out_old_value);

/**
 * @brief Get approximate count of elements currently in the skip list
 * @param skiplist Pointer to the skip list
 * @return Element count, or 0 if NULL
 */
size_t cobalt_lfskiplist_size(const cobalt_lfskiplist_t *skiplist);

/**
 * @brief Check whether the skip list is empty
 * @param skiplist Pointer to the skip list
 * @return true if empty or NULL, false otherwise
 */
bool cobalt_lfskiplist_is_empty(const cobalt_lfskiplist_t *skiplist);

/* ---------------- Range Scan & Iterator APIs ---------------- */

/**
 * @brief Create an ordered range scan iterator (pins EBR epoch)
 * @param skiplist Pointer to the skip list
 * @param min_key Inclusive minimum key (pass NULL for unbounded start)
 * @param max_key Inclusive maximum key (pass NULL for unbounded end)
 * @return Pointer to iterator, or NULL on failure
 */
cobalt_lfskiplist_iter_t *cobalt_lfskiplist_iter_create(cobalt_lfskiplist_t *skiplist,
                                                        const void          *min_key,
                                                        const void          *max_key);

/**
 * @brief Step iterator forward to next valid element
 * @param iter Pointer to iterator
 * @param out_key Output location for current key pointer (may be NULL)
 * @param out_value Output location for current value pointer (may be NULL)
 * @return true if an element was yielded, false if iteration completed
 */
bool cobalt_lfskiplist_iter_next(cobalt_lfskiplist_iter_t *iter, void **out_key, void **out_value);

/**
 * @brief Destroy range iterator and release pinned EBR epoch
 * @param iter Pointer to iterator
 */
void cobalt_lfskiplist_iter_destroy(cobalt_lfskiplist_iter_t *iter);

/* ---------------- Explicit EBR Pinning APIs (Optional) ---------------- */

/**
 * @brief Explicitly pin the current thread's EBR epoch for batch operations
 * @param skiplist Pointer to the skip list
 * @return Guard structure holding active handle
 */
cobalt_lfskiplist_guard_t cobalt_lfskiplist_pin(cobalt_lfskiplist_t *skiplist);

/**
 * @brief Release explicit EBR pin
 * @param guard Pointer to guard returned by cobalt_lfskiplist_pin
 */
void cobalt_lfskiplist_unpin(cobalt_lfskiplist_guard_t *guard);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* COBALT_CONTAINER_LFSKIPLIST_H */
