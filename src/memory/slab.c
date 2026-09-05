#include <stdint.h>
/**
 * @file slab.c
 * @brief Object-caching slab allocator implementation
 *
 * @details Implements a slab allocator that pre-allocates fixed-size blocks in "classes".
 *          Each class manages a pool of identically-sized blocks with a free-list for O(1)
 *          allocation and deallocation. Block sizes smaller than a pointer are rounded up
 *          to `sizeof(void *)` to ensure the free-list pointer fits safely. The allocator
 *          uses separate chaining internally — each class maintains its own memory region
 *          and singly-linked free-list embedded within the allocated blocks themselves.
 */

#include "cobalt/memory/allocator.h"
#include "cobalt/memory/slab.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Maximum number of size classes a single slab can manage
 *
 * @details Limits the number of distinct block sizes. Chosen to balance flexibility
 *          with reasonable stack/static allocation for the class array.
 */
#define COBALT_SLAB_MAX_CLASSES 16

/**
 * @brief Internal slab class descriptor
 *
 * @details Each class manages a contiguous block of identically-sized allocations.
 *          The free-list is embedded in the free blocks themselves: each free block's
 *          first `sizeof(void *)` bytes store a pointer to the next free block.
 */
typedef struct {
    size_t block_size;  /**< Byte size of each block in this class */
    size_t block_count; /**< Total number of blocks allocated for this class */
    size_t free_count;  /**< Number of currently free (unallocated) blocks */
    void  *memory;      /**< Base address of the contiguous memory block for this class */
    void  *free_list;   /**< Head of the singly-linked free-list of available blocks */
} cobalt_slab_class_t;

/**
 * @brief Internal slab allocator structure
 *
 * @details Contains an array of class descriptors and the allocator used for internal
 *          allocations. The `class_count` field tracks how many of the `classes` array
 *          entries are in use.
 */
struct cobalt_slab {
    cobalt_slab_class_t
           classes[COBALT_SLAB_MAX_CLASSES]; /**< Pre-allocated class descriptor array */
    size_t class_count;                      /**< Number of active size classes */
    cobalt_allocator_t *alloc;               /**< Backing allocator for slab memory */
};

/* ========================================================================= */
/* Internal helpers                                                          */
/* ========================================================================= */

/**
 * @brief Find the smallest class that can satisfy a size request
 *
 * @details Performs a linear scan over active classes. Because classes are created
 *          in ascending order of block size, the first class whose `block_size`
 *          is >= `size` is the best fit.
 *
 * @param slab Pointer to the slab allocator
 * @param size Requested allocation size in bytes
 * @return Index of the matching class, or -1 if no class can satisfy the request
 */
static int slab_find_class(cobalt_slab_t *slab, size_t size)
{
    for (size_t i = 0; i < slab->class_count; i++) {
        if (size <= slab->classes[i].block_size) {
            return (int)i;
        }
    }
    return -1;
}

/* ========================================================================= */
/* Public API                                                                */
/* ========================================================================= */

/**
 * @brief Create a new slab allocator using the system allocator
 *
 * @param sizes      Array of desired block sizes (one per class), in bytes
 * @param counts     Array of block counts (one per class); a count of 0 defaults to 16
 * @param class_count Number of size classes (must be <= COBALT_SLAB_MAX_CLASSES)
 * @return Pointer to the created slab, or NULL on failure
 */
cobalt_slab_t *cobalt_slab_create(const size_t *sizes, const size_t *counts, size_t class_count)
{
    return cobalt_slab_create_with_allocator(
        sizes, counts, class_count, cobalt_allocator_get_system());
}

/**
 * @brief Create a new slab allocator with a custom backing allocator
 *
 * @details Validates all input parameters, allocates the slab control structure,
 *          then iterates over each size class to allocate and initialize its
 *          memory block and embedded free-list.
 *
 * @param sizes      Array of desired block sizes (one per class), in bytes
 * @param counts     Array of block counts (one per class); a count of 0 defaults to 16
 * @param class_count Number of size classes (must be <= COBALT_SLAB_MAX_CLASSES)
 * @param alloc      Custom allocator (must not be NULL)
 * @return Pointer to the created slab, or NULL on failure
 *
 * @note If any class allocation fails, the partially-constructed slab is destroyed
 *       and NULL is returned. Block sizes smaller than `sizeof(void *)` are rounded
 *       up to ensure the free-list pointer fits within a single block.
 */
cobalt_slab_t *cobalt_slab_create_with_allocator(const size_t       *sizes,
                                                 const size_t       *counts,
                                                 size_t              class_count,
                                                 cobalt_allocator_t *alloc)
{
    if (!sizes || !counts || class_count == 0 || class_count > COBALT_SLAB_MAX_CLASSES || !alloc) {
        return NULL;
    }

    cobalt_slab_t *slab = (cobalt_slab_t *)alloc->alloc(alloc, sizeof(cobalt_slab_t));
    if (!slab) {
        return NULL;
    }
    memset(slab, 0, sizeof(cobalt_slab_t));

    for (size_t i = 0; i < class_count; i++) {
        size_t bs = sizes[i];
        if (bs < sizeof(void *)) {
            bs = sizeof(void *);
        }
        size_t cnt = counts[i];
        if (cnt == 0) {
            cnt = 16;
        }

        cobalt_slab_class_t *cls = &slab->classes[i];
        cls->block_size          = bs;
        cls->block_count         = cnt;
        cls->free_count          = cnt;
        cls->memory              = alloc->alloc(alloc, bs * cnt);
        if (!cls->memory) {
            cobalt_slab_destroy(slab);
            return NULL;
        }
        memset(cls->memory, 0, bs * cnt);

        /* Build free-list: link each block's first sizeof(void *) bytes to the next block */
        cls->free_list      = cls->memory;
        unsigned char *base = (unsigned char *)cls->memory;
        for (size_t j = 0; j < cnt - 1; j++) {
            void **next = (void **)(base + j * bs);
            *next       = (void *)(base + (j + 1) * bs);
        }
        void **last = (void **)(base + (cnt - 1) * bs);
        *last       = NULL;
    }

    slab->class_count = class_count;
    slab->alloc       = alloc;
    return slab;
}

/**
 * @brief Destroy a slab allocator and free all associated memory
 *
 * @details Iterates over all active classes and frees each class's memory block,
 *          then frees the slab control structure itself. Does not attempt to track
 *          or free individual outstanding allocations — callers must ensure all
 *          allocated blocks have been returned via `cobalt_slab_free()` first.
 *
 * @param slab Pointer to the slab to destroy; if NULL, no action is taken
 */
void cobalt_slab_destroy(cobalt_slab_t *slab)
{
    if (!slab) {
        return;
    }
    for (size_t i = 0; i < slab->class_count; i++) {
        slab->alloc->free(slab->alloc, slab->classes[i].memory);
    }
    slab->alloc->free(slab->alloc, slab);
}

/**
 * @brief Allocate a block from the slab
 *
 * @details Finds the smallest class whose block size can accommodate `size`,
 *          then pops a block from that class's free-list. Allocated blocks are
 *          zero-initialized for safety. Returns NULL if the slab is NULL, `size`
 *          is 0, no class can satisfy the request, or the chosen class is exhausted.
 *
 * @param slab Pointer to the slab allocator
 * @param size Requested allocation size in bytes
 * @return Pointer to the allocated block, or NULL on failure
 */
void *cobalt_slab_alloc(cobalt_slab_t *slab, size_t size)
{
    if (!slab || size == 0) {
        return NULL;
    }

    int idx = slab_find_class(slab, size);
    if (idx < 0) {
        return NULL;
    }

    cobalt_slab_class_t *cls = &slab->classes[(size_t)idx];
    if (cls->free_count == 0) {
        return NULL;
    }

    void *block    = cls->free_list;
    cls->free_list = *(void **)block;
    cls->free_count--;
    memset(block, 0, cls->block_size);
    return block;
}

/**
 * @brief Return a block to the slab
 *
 * @details Identifies which class owns `ptr` by checking if the pointer falls
 *          within a class's memory region and is properly aligned to that class's
 *          block size. If found, the block is pushed onto the class's free-list.
 *          Silently ignores NULL pointers or pointers that do not belong to any
 *          class in this slab.
 *
 * @param slab Pointer to the slab allocator
 * @param ptr  Pointer to the block to free; if NULL, no action is taken
 */
void cobalt_slab_free(cobalt_slab_t *slab, void *ptr)
{
    if (!slab || !ptr) {
        return;
    }

    for (size_t i = 0; i < slab->class_count; i++) {
        cobalt_slab_class_t *cls    = &slab->classes[i];
        unsigned char       *base   = (unsigned char *)cls->memory;
        uintptr_t            offset = (uintptr_t)ptr - (uintptr_t)base;
        if (offset >= cls->block_size * cls->block_count) {
            continue;
        }
        if (offset % cls->block_size != 0) {
            continue;
        }
        *(void **)ptr  = cls->free_list;
        cls->free_list = ptr;
        cls->free_count++;
        return;
    }
}
