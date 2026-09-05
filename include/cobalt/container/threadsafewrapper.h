#ifndef THREADSAFE_WRAPPER_H
#define THREADSAFE_WRAPPER_H

/**
 * @file threadsafewrapper.h
 * @brief Thread-safe container wrappers
 * @details Wraps core containers with mutex locking for concurrent access.
 *          All operations acquire the mutex before touching the underlying
 *          container. Destroy the wrapper (not the inner container directly)
 *          to ensure the mutex is released.
 *
 * @note Operations that hold the lock for a long time (e.g., full-traversal
 *       iterators) are intentionally excluded. For complex concurrent workloads,
 *       use a higher-level coordination strategy.
 *
 * @defgroup ThreadSafeContainers Thread-Safe Containers
 * @{
 */

#include "cobalt/container/hashmap.h"
#include "cobalt/container/list.h"
#include "cobalt/container/vector.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/platform/thread.h"
#include <stddef.h>

/* ======================================================================== */
/* Thread-Safe Vector                                                        */
/* ======================================================================== */

/**
 * @brief Opaque thread-safe vector type
 */
typedef struct cobalt_tsvector cobalt_tsvector_t;

/**
 * @brief Create a thread-safe vector
 * @param initial_capacity Initial capacity. If 0, grows on first push.
 * @return Pointer to the new thread-safe vector, or NULL on failure
 */
cobalt_tsvector_t *cobalt_tsvector_create(size_t initial_capacity);

/**
 * @brief Create a thread-safe vector with a custom allocator
 * @param initial_capacity Initial capacity
 * @param alloc Custom allocator (must not be NULL)
 * @return Pointer to the new thread-safe vector, or NULL on failure
 */
cobalt_tsvector_t *cobalt_tsvector_create_with_allocator(size_t              initial_capacity,
                                                         cobalt_allocator_t *alloc);

/**
 * @brief Destroy a thread-safe vector and free all memory
 */
void cobalt_tsvector_destroy(cobalt_tsvector_t *vec);

/**
 * @brief Add an element to the end (thread-safe)
 * @return 0 on success, -1 on failure
 */
int cobalt_tsvector_push(cobalt_tsvector_t *vec, void *item);

/**
 * @brief Get the element at index (thread-safe)
 * @return Element pointer, or NULL if index is out of bounds
 */
void *cobalt_tsvector_get(const cobalt_tsvector_t *vec, size_t index);

/**
 * @brief Set the element at index (thread-safe)
 * @return 0 on success, -1 on failure
 */
int cobalt_tsvector_set(cobalt_tsvector_t *vec, size_t index, void *item);

/**
 * @brief Remove the element at index (thread-safe)
 */
void cobalt_tsvector_remove_at(cobalt_tsvector_t *vec, size_t index);

/**
 * @brief Get the number of elements (thread-safe)
 */
size_t cobalt_tsvector_size(const cobalt_tsvector_t *vec);

/**
 * @brief Check if empty (thread-safe)
 */
int cobalt_tsvector_is_empty(const cobalt_tsvector_t *vec);

/**
 * @brief Get current capacity (thread-safe)
 */
size_t cobalt_tsvector_capacity(const cobalt_tsvector_t *vec);

/**
 * @brief Reserve capacity (thread-safe)
 */
int cobalt_tsvector_reserve(cobalt_tsvector_t *vec, size_t n);

/**
 * @brief Shrink to fit (thread-safe)
 */
int cobalt_tsvector_shrink_to_fit(cobalt_tsvector_t *vec);

/* ======================================================================== */
/* Thread-Safe HashMap                                                       */
/* ======================================================================== */

/**
 * @brief Opaque thread-safe hashmap type
 */
typedef struct cobalt_tshashmap cobalt_tshashmap_t;

/**
 * @brief Create a thread-safe hashmap
 * @param initial_buckets Initial bucket count. If 0, grows lazily.
 * @return Pointer to the new thread-safe hashmap, or NULL on failure
 */
cobalt_tshashmap_t *cobalt_tshashmap_create(size_t initial_buckets);

/**
 * @brief Create a thread-safe hashmap with a custom allocator
 * @param initial_buckets Initial bucket count
 * @param alloc Custom allocator (must not be NULL)
 * @return Pointer to the new thread-safe hashmap, or NULL on failure
 */
cobalt_tshashmap_t *cobalt_tshashmap_create_with_allocator(size_t              initial_buckets,
                                                           cobalt_allocator_t *alloc);

/**
 * @brief Destroy a thread-safe hashmap
 */
void cobalt_tshashmap_destroy(cobalt_tshashmap_t *map);

/**
 * @brief Insert or update a key-value pair (thread-safe)
 * @return 0 on success, -1 on failure
 */
int cobalt_tshashmap_put(cobalt_tshashmap_t *map, const char *key, void *value);

/**
 * @brief Get a value by key (thread-safe)
 * @return Value pointer, or NULL if not found
 */
void *cobalt_tshashmap_get(const cobalt_tshashmap_t *map, const char *key);

/**
 * @brief Remove a key (thread-safe)
 * @return 0 on success, -1 if not found
 */
int cobalt_tshashmap_remove(cobalt_tshashmap_t *map, const char *key);

/**
 * @brief Check if key exists (thread-safe)
 * @return 1 if present, 0 if not
 */
int cobalt_tshashmap_contains(const cobalt_tshashmap_t *map, const char *key);

/**
 * @brief Get the number of entries (thread-safe)
 */
size_t cobalt_tshashmap_size(const cobalt_tshashmap_t *map);

/**
 * @brief Get current bucket count (thread-safe)
 */
size_t cobalt_tshashmap_capacity(const cobalt_tshashmap_t *map);

/* ======================================================================== */
/* Thread-Safe List                                                          */
/* ======================================================================== */

/**
 * @brief Opaque thread-safe list type
 */
typedef struct cobalt_tslist cobalt_tslist_t;

/**
 * @brief Create a thread-safe list
 * @return Pointer to the new thread-safe list, or NULL on failure
 */
cobalt_tslist_t *cobalt_tslist_create(void);

/**
 * @brief Create a thread-safe list with a custom allocator
 * @param alloc Custom allocator (must not be NULL)
 * @return Pointer to the new thread-safe list, or NULL on failure
 */
cobalt_tslist_t *cobalt_tslist_create_with_allocator(cobalt_allocator_t *alloc);

/**
 * @brief Destroy a thread-safe list
 */
void cobalt_tslist_destroy(cobalt_tslist_t *list);

/**
 * @brief Push to front (thread-safe)
 */
int cobalt_tslist_push_front(cobalt_tslist_t *list, void *item);

/**
 * @brief Push to back (thread-safe)
 */
int cobalt_tslist_push_back(cobalt_tslist_t *list, void *item);

/**
 * @brief Pop from front (thread-safe)
 * @return Element pointer, or NULL if empty
 */
void *cobalt_tslist_pop_front(cobalt_tslist_t *list);

/**
 * @brief Pop from back (thread-safe)
 * @return Element pointer, or NULL if empty
 */
void *cobalt_tslist_pop_back(cobalt_tslist_t *list);

/**
 * @brief Get element at index (thread-safe)
 * @return Element pointer, or NULL if out of bounds
 */
void *cobalt_tslist_get(const cobalt_tslist_t *list, size_t index);

/**
 * @brief Remove element at index (thread-safe)
 * @return 0 on success, -1 on failure
 */
int cobalt_tslist_remove_at(cobalt_tslist_t *list, size_t index);

/**
 * @brief Get the number of elements (thread-safe)
 */
size_t cobalt_tslist_size(const cobalt_tslist_t *list);

/**
 * @brief Check if empty (thread-safe)
 */
int cobalt_tslist_is_empty(const cobalt_tslist_t *list);

/** @} */

#endif /* THREADSAFE_WRAPPER_H */
