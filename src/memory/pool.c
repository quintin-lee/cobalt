/**
 * @file pool.c
 * @brief Fixed-size block pool allocator implementation
 */

#include "cobalt/memory/pool.h"
#include "cobalt/memory/allocator.h"
#include <stdint.h>
#include <string.h>

/**
 * @brief Fixed-size block pool structure
 */
struct cobalt_pool {
    size_t              block_size;  /**< Byte size of each block */
    size_t              block_count; /**< Total number of blocks in the pool */
    size_t              free_count;  /**< Number of currently unallocated blocks */
    void               *memory;      /**< Contiguous block of memory */
    void               *free_list;   /**< Linked list of free blocks */
    cobalt_allocator_t *alloc;       /**< Allocator instance */
};

/**
 * @brief Create a fixed-size block pool using the system allocator
 * @param block_size Size of each block in bytes
 * @param block_count Number of blocks in the pool
 * @return Pointer to the created pool, or NULL on failure
 */
cobalt_pool_t *cobalt_pool_create(size_t block_size, size_t block_count)
{
    return cobalt_pool_create_with_allocator(
        block_size, block_count, cobalt_allocator_get_system());
}

/**
 * @brief Create a fixed-size block pool with a custom backing allocator
 * @details Block sizes smaller than a pointer are rounded up so the free-list link fits.
 * The blocks are chained into a free-list up front.
 * @param block_size Size of each block in bytes (must not be 0)
 * @param block_count Number of blocks in the pool (must not be 0)
 * @param alloc Custom allocator (must not be NULL)
 * @return Pointer to the created pool, or NULL on invalid arguments or allocation failure
 */
cobalt_pool_t *
cobalt_pool_create_with_allocator(size_t block_size, size_t block_count, cobalt_allocator_t *alloc)
{
    if (block_size == 0 || block_count == 0 || !alloc) {
        return NULL;
    }
    if (block_size < sizeof(void *)) {
        block_size = sizeof(void *);
    }

    cobalt_pool_t *pool = (cobalt_pool_t *)alloc->alloc(alloc, sizeof(cobalt_pool_t));
    if (!pool) {
        return NULL;
    }

    pool->memory = alloc->alloc(alloc, block_size * block_count);
    if (!pool->memory) {
        alloc->free(alloc, pool);
        return NULL;
    }

    pool->block_size  = block_size;
    pool->block_count = block_count;
    pool->free_count  = block_count;
    pool->alloc       = alloc;

    /* Build free-list: chain blocks together */
    pool->free_list     = pool->memory;
    unsigned char *base = (unsigned char *)pool->memory;
    for (size_t i = 0; i < block_count - 1; i++) {
        void **next = (void **)(base + i * block_size);
        *next       = (void *)(base + (i + 1) * block_size);
    }
    /* Last block points to NULL */
    void **last = (void **)(base + (block_count - 1) * block_size);
    *last       = NULL;

    return pool;
}

/**
 * @brief Destroy a pool and release its memory
 * @details Does not track outstanding allocations; all blocks must be freed first
 * @param pool Pointer to the pool; if NULL, no action is taken
 */
void cobalt_pool_destroy(cobalt_pool_t *pool)
{
    if (!pool) {
        return;
    }
    pool->alloc->free(pool->alloc, pool->memory);
    pool->alloc->free(pool->alloc, pool);
}

/**
 * @brief Allocate one zero-initialized block from the pool
 * @param pool Pointer to the pool
 * @return Pointer to the block, or NULL if the pool is NULL or exhausted
 */
void *cobalt_pool_alloc(cobalt_pool_t *pool)
{
    if (!pool || pool->free_count == 0) {
        return NULL;
    }

    /* Pop from free-list head */
    void *block     = pool->free_list;
    pool->free_list = *(void **)block;
    pool->free_count--;

    /* Zero out the block before returning */
    memset(block, 0, pool->block_size);
    return block;
}

/**
 * @brief Return a block to the pool
 * @details Validates that the pointer falls inside the pool region and is block-aligned;
 * foreign or misaligned pointers are silently ignored
 * @param pool Pointer to the pool
 * @param ptr Block to free; if NULL, no action is taken
 */
void cobalt_pool_free(cobalt_pool_t *pool, void *ptr)
{
    if (!pool || !ptr) {
        return;
    }

    /* Validate pointer is within pool memory range */
    unsigned char *base   = (unsigned char *)pool->memory;
    uintptr_t      offset = (uintptr_t)ptr - (uintptr_t)base;
    if (offset >= pool->block_size * pool->block_count) {
        return; /* Not from this pool */
    }
    if (offset % pool->block_size != 0) {
        return; /* Not aligned to block boundary */
    }

    /* Push onto free-list head */
    *(void **)ptr   = pool->free_list;
    pool->free_list = ptr;
    pool->free_count++;
}

/**
 * @brief Check whether the pool has no free blocks left
 * @param pool Pointer to the pool (NULL counts as full)
 * @return 1 if full or NULL, 0 otherwise
 */
int cobalt_pool_is_full(const cobalt_pool_t *pool)
{
    return pool ? (pool->free_count == 0) : 1;
}

/**
 * @brief Get the number of currently free blocks
 * @param pool Pointer to the pool (NULL yields 0)
 * @return Number of free blocks
 */
size_t cobalt_pool_free_count(const cobalt_pool_t *pool)
{
    return pool ? pool->free_count : 0;
}
