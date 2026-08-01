#ifndef MEMORY_ALLOCATOR_H
#define MEMORY_ALLOCITOR_H

/**
 * @file allocator.h
 * @brief Memory allocator interface
 */

#include <stddef.h>

typedef struct cobalt_allocator
{
    void* (*alloc)(struct cobalt_allocator* self, size_t size);
    void (*free)(struct cobalt_allocator* self, void* ptr);
    void* (*realloc)(struct cobalt_allocator* self, void* ptr, size_t new_size);
} cobalt_allocator_t;

/* Default system allocator */
cobalt_allocator_t* cobalt_allocator_get_system(void);

/* Allocator-specific allocation */
void* cobalt_allocator_alloc(cobalt_allocator_t* self, size_t size);
void cobalt_allocator_free(cobalt_allocator_t* self, void* ptr);
void* cobalt_allocator_realloc(cobalt_allocator_t* self, void* ptr, size_t new_size);

#endif /* MEMORY_ALLOCATOR_H */
