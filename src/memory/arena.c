#include "cobalt/memory/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

struct cobalt_arena
{
    void* buffer;
    size_t size;
    size_t used;
    size_t capacity;
};

cobalt_arena_t* cobalt_arena_create(size_t initial_size)
{
    cobalt_arena_t* arena = malloc(sizeof(cobalt_arena_t));
    if (!arena)
        {
            return NULL;
        }

    arena->buffer = malloc(initial_size);
    if (!arena->buffer)
        {
            free(arena);
            return NULL;
        }

    arena->size = initial_size;
    arena->used = 0;
    arena->capacity = initial_size;
    return arena;
}

void cobalt_arena_destroy(cobalt_arena_t* arena)
{
    if (arena)
        {
            free(arena->buffer);
            free(arena);
        }
}

void* cobalt_arena_alloc(cobalt_arena_t* arena, size_t size)
{
    if (!arena)
        return NULL;

    size_t aligned_size = (size + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);

    if (arena->used + aligned_size > arena->capacity)
        {
            size_t new_capacity = arena->capacity * 2;
            void* new_buffer = realloc(arena->buffer, new_capacity);
            if (!new_buffer)
                return NULL;
            arena->buffer = new_buffer;
            arena->capacity = new_capacity;
        }

    void* ptr = (char*)arena->buffer + arena->used;
    arena->used += aligned_size;
    return ptr;
}

void cobalt_arena_reset(cobalt_arena_t* arena)
{
    if (arena)
        {
            arena->used = 0;
        }
}
