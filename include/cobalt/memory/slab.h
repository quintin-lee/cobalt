#ifndef MEMORY_SLAB_H
#define MEMORY_SLAB_H

/**
 * @file slab.h
 * @brief Object-caching slab allocator
 * @details Manages multiple fixed-size object classes. Each class maintains its own
 *          free-list for O(1) allocation and deallocation. Ideal for frequent alloc/free
 *          cycles of a known set of object sizes.
 *
 * @defgroup Slab Slab allocator module
 * @ingroup Memory
 * @{
 */

#include "cobalt/memory/allocator.h"
#include <stddef.h>

/**
 * @brief Opaque slab allocator type
 */
typedef struct cobalt_slab cobalt_slab_t;

/**
 * @brief Create a new slab allocator with one or more size classes
 * @param sizes       Array of object sizes (in bytes) for each class
 * @param counts      Array of block counts for each corresponding size class
 * @param class_count Number of size classes
 * @return Newly created slab, or NULL on failure
 */
cobalt_slab_t *cobalt_slab_create(const size_t *sizes, const size_t *counts, size_t class_count);

/**
 * @brief Create a new slab allocator with custom allocator
 * @param sizes       Array of object sizes (in bytes) for each class
 * @param counts      Array of block counts for each corresponding size class
 * @param class_count Number of size classes
 * @param alloc       Custom allocator to use (must not be NULL)
 * @return Newly created slab, or NULL on failure
 */
cobalt_slab_t *cobalt_slab_create_with_allocator(const size_t       *sizes,
                                                 const size_t       *counts,
                                                 size_t              class_count,
                                                 cobalt_allocator_t *alloc);

/**
 * @brief Destroy the slab and free all associated memory
 * @param slab Slab to destroy. No-op if NULL.
 */
void cobalt_slab_destroy(cobalt_slab_t *slab);

/**
 * @brief Allocate an object of the specified size class
 * @param slab     Slab to allocate from
 * @param size     Requested object size (must match a registered class, or be smaller)
 * @return Pointer to the allocated object, or NULL if no matching class or slab is NULL
 */
void *cobalt_slab_alloc(cobalt_slab_t *slab, size_t size);

/**
 * @brief Free an object back to its size class
 * @param slab  Slab the object was allocated from
 * @param ptr   Object to free

 * @thread_safety The container is **not thread-safe**. Concurrent access from
 *                multiple threads without external synchronization leads to
 *                data races and undefined behavior. Use a mutex or other
 *                synchronization primitive to protect shared containers.
 *                Reference-counted objects (@ref cobalt_object_t) use atomic
 *                operations and are thread-safe for ref-count operations.
 */
void cobalt_slab_free(cobalt_slab_t *slab, void *ptr);

/** @} */

#endif /* MEMORY_SLAB_H */
