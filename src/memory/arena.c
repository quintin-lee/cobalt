/**
 * @file arena.c
 * @brief Implementation of the arena (region-based) memory allocator
 *
 * @details Provides a high-performance linear "bump pointer" allocator. The arena
 *          allocates a large contiguous buffer upfront and satisfies requests by
 *          linearly advancing a cursor. Memory is never individually freed during
 *          the arena's lifetime — instead, `cobalt_arena_reset()` rewinds the
 *          cursor in O(1), making the entire buffer available for reuse.
 *
 *          Allocation requests are rounded up to `alignof(max_align_t)` to ensure
 *          the returned pointer is suitably aligned for any C data type. When the
 *          current buffer is exhausted, the arena doubles its capacity via `realloc`.
 */

#include "cobalt/memory/arena.h"
#include "cobalt/memory/allocator.h"
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Internal arena control structure
 *
 * @details Maintains the state for a single arena instance. Note the distinction
 *          between `size` (the initial/logical size, currently unused in allocation
 *          logic) and `capacity` (the actual bytes backing the arena's buffer).
 */
struct cobalt_arena {
    void               *buffer;   /**< Base address of the underlying memory block */
    size_t              size;     /**< Initial or logical size; retained for introspection */
    size_t              used;     /**< Number of bytes already allocated (bump-pointer cursor) */
    size_t              capacity; /**< Total capacity of the underlying buffer in bytes */
    cobalt_allocator_t *alloc;    /**< Backing allocator used for buffer growth and destruction */
};

/* ========================================================================= */
/* Lifecycle                                                                 */
/* ========================================================================= */

/**
 * @brief Create a new memory arena using the system allocator
 *
 * @param initial_size Initial capacity of the arena in bytes
 * @return Pointer to the created arena, or NULL on failure
 */
cobalt_arena_t *cobalt_arena_create(size_t initial_size)
{
    return cobalt_arena_create_with_allocator(initial_size, cobalt_allocator_get_system());
}

/**
 * @brief Create a new memory arena with a custom backing allocator
 *
 * @details Allocates the arena control structure and the underlying memory buffer
 *          in sequence. If the buffer allocation fails, the control structure is
 *          freed before returning NULL to avoid leaks.
 *
 * @param initial_size Initial capacity of the arena in bytes
 * @param alloc        Custom allocator (must not be NULL)
 * @return Pointer to the created arena, or NULL on failure
 */
cobalt_arena_t *cobalt_arena_create_with_allocator(size_t initial_size, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    cobalt_arena_t *arena = (cobalt_arena_t *)alloc->alloc(alloc, sizeof(cobalt_arena_t));
    if (!arena) {
        return NULL;
    }

    arena->buffer = alloc->alloc(alloc, initial_size);
    if (!arena->buffer) {
        alloc->free(alloc, arena);
        return NULL;
    }

    arena->size     = initial_size;
    arena->used     = 0;
    arena->capacity = initial_size;
    arena->alloc    = alloc;
    return arena;
}

/**
 * @brief Destroy an arena and release all underlying memory
 *
 * @details Frees the backing buffer first, then the arena control structure.
 *          After this call, all pointers previously returned by `cobalt_arena_alloc()`
 *          from this arena are invalid.
 *
 * @param arena Pointer to the arena to destroy; if NULL, no action is taken
 */
void cobalt_arena_destroy(cobalt_arena_t *arena)
{
    if (arena) {
        arena->alloc->free(arena->alloc, arena->buffer);
        arena->alloc->free(arena->alloc, arena);
    }
}

/* ========================================================================= */
/* Allocation                                                               */
/* ========================================================================= */

/**
 * @brief Allocate memory from the arena
 *
 * @details Uses a linear bump-pointer strategy:
 *          1. Round `size` up to `alignof(max_align_t)` via bitwise arithmetic
 *          2. If the rounded size exceeds remaining capacity, double the buffer
 *          3. Return the current cursor position and advance `used`
 *
 *          The alignment formula `(size + align - 1) & ~(align - 1)` rounds up
 *          without branches or division.
 *
 * @param arena Pointer to the arena
 * @param size  Number of bytes to allocate
 * @return Pointer to the allocated memory, or NULL if the arena is NULL or
 *         reallocation fails
 */
void *cobalt_arena_alloc(cobalt_arena_t *arena, size_t size)
{
    if (!arena) {
        return NULL;
    }

    /* Align size to max_align_t so the returned pointer works for any C type */
    size_t aligned_size = (size + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);

    if (arena->used + aligned_size > arena->capacity) {
        size_t new_capacity = arena->capacity * 2;
        void  *new_buffer   = arena->alloc->realloc(arena->alloc, arena->buffer, new_capacity);
        if (!new_buffer) {
            return NULL;
        }
        arena->buffer   = new_buffer;
        arena->capacity = new_capacity;
    }

    void *ptr = (char *)arena->buffer + arena->used;
    arena->used += aligned_size;
    return ptr;
}

/**
 * @brief Reset the arena, making all previously allocated memory available again
 *
 * @details Simply rewinds the `used` cursor to 0. The underlying buffer is NOT
 *          returned to the system — it is retained for fast reuse. This makes
 *          reset() an O(1) operation.
 *
 *          @warning Any pointers previously returned by `cobalt_arena_alloc()` become
 *          dangling after this call. The old data remains in the buffer but will be
 *          overwritten by subsequent allocations.
 *
 * @param arena Pointer to the arena; if NULL, no action is taken
 */
void cobalt_arena_reset(cobalt_arena_t *arena)
{
    if (arena) {
        arena->used = 0;
    }
}
