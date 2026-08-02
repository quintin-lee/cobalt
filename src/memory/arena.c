/**
 * @file arena.c
 * @brief Implementation of the memory arena
 * @details Provides a high-performance linear memory allocation mechanism, automatically handling
 * memory alignment and on-demand reallocation.
 */

#include "cobalt/memory/arena.h"
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Internal arena structure
 */
struct cobalt_arena {
    void  *buffer;   /**< Base address of the underlying memory block */
    size_t size;     /**< Field currently not in actual use, represents initial or logical size */
    size_t used;     /**< Number of bytes currently allocated (used) */
    size_t capacity; /**< Total capacity of the arena's underlying memory block (in bytes) */
};

/*
 * @brief Create a memory arena of a specified initial size
 * @details Allocates the arena control structure and the underlying memory block buffer.
 */
cobalt_arena_t *cobalt_arena_create(size_t initial_size)
{
    cobalt_arena_t *arena = malloc(sizeof(cobalt_arena_t));
    if (!arena) {
        return NULL; // Control structure allocation failed
    }

    arena->buffer = malloc(initial_size);
    if (!arena->buffer) {
        free(arena); // Reclaim control structure
        return NULL; // Underlying memory block allocation failed
    }

    // Initialize parameters
    arena->size     = initial_size;
    arena->used     = 0;
    arena->capacity = initial_size;
    return arena;
}

/*
 * @brief Destroy the arena
 * @details Frees the underlying memory buffer as well as the arena structure itself.
 */
void cobalt_arena_destroy(cobalt_arena_t *arena)
{
    if (arena) {
        free(arena->buffer); // Free the large memory block
        free(arena);         // Free the structure
    }
}

/*
 * @brief Allocate memory within the arena
 * @details Uses a linear bump pointer approach to allocate memory, ensuring alignment for
 * `max_align_t`. If the current capacity is insufficient to accommodate the requested aligned size,
 * it doubles the capacity.
 */
void *cobalt_arena_alloc(cobalt_arena_t *arena, size_t size)
{
    if (!arena) {
        return NULL;
    }

    // Calculate aligned size. Uses `alignof(max_align_t)` to ensure the address is suitable for any
    // basic data type. Bitwise operations achieve rounding up for alignment.
    size_t aligned_size = (size + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);

    // Check if there is enough remaining space
    if (arena->used + aligned_size > arena->capacity) {
        // Insufficient capacity, double the total capacity
        size_t new_capacity = arena->capacity * 2;
        void  *new_buffer   = realloc(arena->buffer, new_capacity);
        if (!new_buffer) {
            return NULL; // Reallocation failed
        }
        arena->buffer   = new_buffer;
        arena->capacity = new_capacity;
    }

    // The allocated address is offset from the start of the existing buffer by the number of used
    // bytes
    void *ptr = (char *)arena->buffer + arena->used;
    // Advance the used cursor
    arena->used += aligned_size;
    return ptr;
}

/*
 * @brief Reset the arena
 * @details A very lightweight operation, just zeroes out the `used` pointer, logically freeing all
 * allocated memory.
 */
void cobalt_arena_reset(cobalt_arena_t *arena)
{
    if (arena) {
        arena->used = 0; // Reset the cursor, the next allocation will overwrite old data
    }
}
