/**
 * @file pool.c
 * @brief Fixed-size block pool allocator implementation
 */

#include "cobalt/memory/pool.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct cobalt_pool {
    size_t block_size;
    size_t block_count;
    size_t free_count;
    void  *memory;    /**< Contiguous block of memory */
    void  *free_list; /**< Linked list of free blocks */
};

cobalt_pool_t *cobalt_pool_create(size_t block_size, size_t block_count)
{
    if (block_size == 0 || block_count == 0) {
        return NULL;
    }
    if (block_size < sizeof(void *)) {
        block_size = sizeof(void *);
    }

    cobalt_pool_t *pool = (cobalt_pool_t *)malloc(sizeof(cobalt_pool_t));
    if (!pool) {
        return NULL;
    }

    pool->memory = malloc(block_size * block_count);
    if (!pool->memory) {
        free(pool);
        return NULL;
    }

    pool->block_size  = block_size;
    pool->block_count = block_count;
    pool->free_count  = block_count;

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

void cobalt_pool_destroy(cobalt_pool_t *pool)
{
    if (!pool) {
        return;
    }
    free(pool->memory);
    free(pool);
}

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

int cobalt_pool_is_full(const cobalt_pool_t *pool)
{
    return pool ? (pool->free_count == 0) : 1;
}

size_t cobalt_pool_free_count(const cobalt_pool_t *pool)
{
    return pool ? pool->free_count : 0;
}
