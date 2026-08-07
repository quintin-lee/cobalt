#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

/**
 * @file pool.h
 * @brief Fixed-size block pool allocator
 * @details Pre-allocates a contiguous region of memory divided into equal-sized blocks.
 *          Supports fast O(1) alloc/free via an internal free-list. Ideal for frequent
 *          small allocations of a known fixed size.
 *
 * @defgroup Pool Pool allocator module
 * @ingroup Memory
 * @{
 */

#include "cobalt/memory/allocator.h"
#include <stddef.h>

/**
 * @brief Opaque pool allocator type
 */
typedef struct cobalt_pool cobalt_pool_t;

/**
 * @brief Create a new fixed-size block pool
 * @param block_size   Size of each block in bytes (must be >= sizeof(void*) for free-list)
 * @param block_count  Number of blocks to pre-allocate
 * @return Newly created pool, or NULL on failure
 */
cobalt_pool_t *cobalt_pool_create(size_t block_size, size_t block_count);

/**
 * @brief Create a new fixed-size block pool using a custom allocator
 * @param block_size   Size of each block in bytes (must be >= sizeof(void*) for free-list)
 * @param block_count  Number of blocks to pre-allocate
 * @param alloc        Custom allocator to use (must not be NULL)
 * @return Newly created pool, or NULL on failure
 */
cobalt_pool_t *
cobalt_pool_create_with_allocator(size_t block_size, size_t block_count, cobalt_allocator_t *alloc);

/**
 * @brief Destroy the pool and free all associated memory
 * @param pool Pool to destroy. No-op if NULL.
 */
void cobalt_pool_destroy(cobalt_pool_t *pool);

/**
 * @brief Allocate a block from the pool
 * @param pool Pool to allocate from
 * @return Pointer to a block, or NULL if pool is full or pool is NULL
 */
void *cobalt_pool_alloc(cobalt_pool_t *pool);

/**
 * @brief Return a block to the pool
 * @param pool Pool the block was allocated from
 * @param ptr  Block to return (must have been allocated from this pool)
 */
void cobalt_pool_free(cobalt_pool_t *pool, void *ptr);

/**
 * @brief Check if the pool has any free blocks remaining
 * @param pool Pool to check
 * @return 1 if pool is full (no free blocks), 0 otherwise
 */
int cobalt_pool_is_full(const cobalt_pool_t *pool);

/**
 * @brief Get the number of free blocks remaining
 * @param pool Pool to query
 * @return Number of available blocks
 */
size_t cobalt_pool_free_count(const cobalt_pool_t *pool);

/** @} */

#endif /* MEMORY_POOL_H */
