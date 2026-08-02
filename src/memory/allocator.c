#include "cobalt/memory/allocator.h"
#include <stdlib.h>

/* Wrapper functions for system allocator vtable - self is ignored */
static void *sys_alloc(cobalt_allocator_t *self, size_t size)
{
    (void)self;
    return malloc(size);
}

static void sys_free(cobalt_allocator_t *self, void *ptr)
{
    (void)self;
    free(ptr);
}

static void *sys_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
{
    (void)self;
    return realloc(ptr, new_size);
}

/* System allocator instance (read-only, shared) */
static const cobalt_allocator_t system_allocator = {
    .alloc = sys_alloc, .free = sys_free, .realloc = sys_realloc};

cobalt_allocator_t *cobalt_allocator_get_system(void)
{
    return (cobalt_allocator_t *)&system_allocator;
}

/* Allocator-specific allocation wrappers */
void *cobalt_allocator_alloc(cobalt_allocator_t *self, size_t size)
{
    return self->alloc(self, size);
}

void cobalt_allocator_free(cobalt_allocator_t *self, void *ptr)
{
    self->free(self, ptr);
}

void *cobalt_allocator_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
{
    return self->realloc(self, ptr, new_size);
}
