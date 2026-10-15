/**
 * @file arena.c
 * @brief Implementation of the arena (region-based) memory allocator
 *
 * @details Provides a high-performance linear "bump pointer" allocator using a
 *          chunked block architecture. The arena allocates chunks of memory upfront
 *          and satisfies requests by linearly advancing a cursor within the active chunk.
 *          When a chunk's remaining space is exhausted, a new chunk is allocated and
 *          linked without moving or reallocating existing chunks.
 *
 *          This guarantees pointer stability: memory addresses returned by
 *          `cobalt_arena_alloc()` never change or become invalid during the lifetime
 *          of the arena.
 *
 *          `cobalt_arena_reset()` rewinds cursors in O(K) (where K is chunk count)
 *          retaining all chunk buffers for immediate reuse without system calls.
 */

#include "cobalt/memory/arena.h"
#include "cobalt/memory/allocator.h"
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

#define COBALT_ARENA_MIN_CHUNK_SIZE 4096

/**
 * @brief Singly-linked memory block backing the arena
 */
typedef struct arena_chunk {
    struct arena_chunk *next;                   /**< Next chunk in the allocated chain */
    size_t              capacity;               /**< Byte capacity of the data buffer */
    size_t              used;                   /**< Number of bytes allocated from this chunk */
    _Alignas(max_align_t) unsigned char data[]; /**< Natural-aligned payload memory */
} arena_chunk_t;

/**
 * @brief Internal arena control structure
 */
struct cobalt_arena {
    arena_chunk_t      *current;            /**< Active chunk serving allocations */
    arena_chunk_t      *chunks;             /**< Head of chunk linked list */
    size_t              default_chunk_size; /**< Default chunk capacity */
    size_t              total_allocated;    /**< Total capacity allocated across all chunks */
    cobalt_allocator_t *alloc;              /**< Backing allocator */
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

    size_t chunk_cap = initial_size > 0 ? initial_size : COBALT_ARENA_MIN_CHUNK_SIZE;
    /* Ensure chunk_cap is aligned to max_align_t */
    chunk_cap = (chunk_cap + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);

    arena_chunk_t *chunk = (arena_chunk_t *)alloc->alloc(alloc, sizeof(arena_chunk_t) + chunk_cap);
    if (!chunk) {
        alloc->free(alloc, arena);
        return NULL;
    }

    chunk->next     = NULL;
    chunk->capacity = chunk_cap;
    chunk->used     = 0;

    arena->current            = chunk;
    arena->chunks             = chunk;
    arena->default_chunk_size = chunk_cap;
    arena->total_allocated    = chunk_cap;
    arena->alloc              = alloc;
    return arena;
}

/**
 * @brief Destroy an arena and release all underlying memory
 *
 * @param arena Pointer to the arena to destroy; if NULL, no action is taken
 */
void cobalt_arena_destroy(cobalt_arena_t *arena)
{
    if (!arena) {
        return;
    }

    arena_chunk_t *curr = arena->chunks;
    while (curr) {
        arena_chunk_t *next = curr->next;
        arena->alloc->free(arena->alloc, curr);
        curr = next;
    }
    arena->alloc->free(arena->alloc, arena);
}

/* ========================================================================= */
/* Allocation                                                               */
/* ========================================================================= */

/**
 * @brief Allocate memory from the arena
 *
 * @param arena Pointer to the arena
 * @param size  Number of bytes to allocate
 * @return Pointer to the allocated memory, or NULL if the arena is NULL or allocation fails
 */
void *cobalt_arena_alloc(cobalt_arena_t *arena, size_t size)
{
    if (!arena || size == 0) {
        return NULL;
    }

    /* Align size to max_align_t so the returned pointer works for any C type */
    size_t aligned_size = (size + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);

    /* Fast path: allocate from current chunk */
    if (arena->current && (arena->current->capacity - arena->current->used >= aligned_size)) {
        void *ptr = arena->current->data + arena->current->used;
        arena->current->used += aligned_size;
        return ptr;
    }

    /* Reuse path after reset: advance to existing next chunk if it fits */
    if (arena->current && arena->current->next &&
        (arena->current->next->capacity >= aligned_size)) {
        arena->current = arena->current->next;
        void *ptr      = arena->current->data + arena->current->used;
        arena->current->used += aligned_size;
        return ptr;
    }

    /* Slow path: allocate new chunk without relocating previously allocated chunks */
    size_t new_cap =
        aligned_size > arena->default_chunk_size ? aligned_size : arena->default_chunk_size;
    new_cap = (new_cap + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);

    arena_chunk_t *new_chunk =
        (arena_chunk_t *)arena->alloc->alloc(arena->alloc, sizeof(arena_chunk_t) + new_cap);
    if (!new_chunk) {
        return NULL;
    }

    new_chunk->capacity = new_cap;
    new_chunk->used     = aligned_size;

    if (arena->current) {
        new_chunk->next      = arena->current->next;
        arena->current->next = new_chunk;
    } else {
        new_chunk->next = arena->chunks;
        arena->chunks   = new_chunk;
    }
    arena->current = new_chunk;
    arena->total_allocated += new_cap;

    return new_chunk->data;
}

/**
 * @brief Reset the arena, making all previously allocated memory available again
 *
 * @details Resets the used cursor of each chunk in the chunk chain back to 0.
 *          Chunk memory is retained for fast reuse, avoiding heap reallocations.
 *
 * @param arena Pointer to the arena; if NULL, no action is taken
 */
void cobalt_arena_reset(cobalt_arena_t *arena)
{
    if (!arena) {
        return;
    }

    arena_chunk_t *curr = arena->chunks;
    while (curr) {
        curr->used = 0;
        curr       = curr->next;
    }
    arena->current = arena->chunks;
}
