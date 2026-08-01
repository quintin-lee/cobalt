#ifndef MEMORY_ARENA_H
#define MEMORY_ARENA_H

/**
 * @file arena.h
 * @brief Memory arena (region-based) allocator
 */

#include <stddef.h>

typedef struct cobalt_arena cobalt_arena_t;

/* Create a new arena */
cobalt_arena_t* cobalt_arena_create(size_t initial_size);

/* Destroy an arena and all its allocations */
void cobalt_arena_destroy(cobalt_arena_t* arena);

/* Allocate from arena */
void* cobalt_arena_alloc(cobalt_arena_t* arena, size_t size);

/* Reset arena (frees all allocations at once) */
void cobalt_arena_reset(cobalt_arena_t* arena);

#endif /* MEMORY_ARENA_H */
