#ifndef MEMORY_ARENA_H
#define MEMORY_ARENA_H

/**
 * @file arena.h
 * @brief Arena (Region-based memory allocator)
 * @details Provides a region-based linear memory allocator. It allocates a large block of memory at
 * once and then linearly sub-allocates from it to satisfy allocation requests. This greatly
 * improves allocation speed and allows for freeing all memory at once via a reset operation,
 * avoiding memory fragmentation and complex individual free logic.
 *
 * @defgroup Arena Arena memory allocation module
 * @ingroup Memory
 * @{
 */

#include <stddef.h>

/**
 * @brief Opaque structure for the arena allocator
 * @details Hides internal state; external manipulation is solely through pointers.
 */
typedef struct cobalt_arena cobalt_arena_t;

/**
 * @brief Create a new memory arena
 * @param initial_size Initial capacity of the arena (in bytes)
 * @return cobalt_arena_t* Pointer to the successfully created arena, or NULL if it fails
 */
cobalt_arena_t *cobalt_arena_create(size_t initial_size);

/**
 * @brief Destroy the memory arena and free all its underlying allocated memory
 * @param arena Pointer to the arena to destroy
 * @note The pointer will become invalid after destruction, and all memory allocated from it will
 * also become invalid.
 */
void cobalt_arena_destroy(cobalt_arena_t *arena);

/**
 * @brief Allocate memory of the specified size from the arena
 * @param arena Pointer to the arena
 * @param size The byte size to allocate
 * @return void* The allocated memory address, aligned to the platform's maximum alignment
 * requirement. Returns NULL if allocation fails (e.g., out of memory and reallocation fails).
 */
void *cobalt_arena_alloc(cobalt_arena_t *arena, size_t size);

/**
 * @brief Reset the arena
 * @param arena Pointer to the arena
 * @details "Frees" all memory allocated from it at once. In reality, it zeroes out the used offset,
 * but the underlying memory is not returned to the OS, allowing for reuse.
 */
void cobalt_arena_reset(cobalt_arena_t *arena);

/** @} */

#endif /* MEMORY_ARENA_H */
