/**
 * @file allocator.c
 * @brief Implementation of the memory allocator interface
 * @details Implements the default allocator based on standard library `malloc`, `free`, and
 * `realloc`, providing unified invocation wrappers.
 */

#include "cobalt/memory/allocator.h"
#include <stdlib.h>

/* Wrapper functions for system allocator vtable - self is ignored */

/**
 * @brief System allocator - memory allocation
 * @details Directly calls standard library `malloc`
 * @param self Allocator instance (ignored, required by the vtable signature)
 * @param size Number of bytes to allocate
 * @return Pointer to the allocated memory, or NULL on failure
 */
static void *sys_alloc(cobalt_allocator_t *self, size_t size)
{
    (void)self; // Suppress unused parameter warning
    return malloc(size);
}

/**
 * @brief System allocator - memory deallocation
 * @details Directly calls standard library `free`
 * @param self Allocator instance (ignored, required by the vtable signature)
 * @param ptr Pointer to the memory to free
 */
static void sys_free(cobalt_allocator_t *self, void *ptr)
{
    (void)self;
    free(ptr);
}

/**
 * @brief System allocator - memory reallocation
 * @details Directly calls standard library `realloc`
 * @param self Allocator instance (ignored, required by the vtable signature)
 * @param ptr Pointer to the memory to resize (may be NULL)
 * @param new_size New size in bytes
 * @return Pointer to the resized memory, or NULL on failure
 */
static void *sys_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
{
    (void)self;
    return realloc(ptr, new_size);
}

/* System allocator instance (read-only, shared) */
static const cobalt_allocator_t system_allocator = {
    .alloc = sys_alloc, .free = sys_free, .realloc = sys_realloc};

/**
 * @brief Get the system allocator instance
 * @details Returns a statically initialized system allocator pointer. The cast removes the const
 * qualifier to adapt to the interface specification.
 * @return Pointer to the shared read-only system allocator
 */
cobalt_allocator_t *cobalt_allocator_get_system(void)
{
    return (cobalt_allocator_t *)&system_allocator;
}

/* Allocator-specific allocation wrappers */

/**
 * @brief Call the allocator's alloc method
 * @param self Allocator instance
 * @param size Number of bytes to allocate
 * @return Pointer to the allocated memory, or NULL on failure
 */
void *cobalt_allocator_alloc(cobalt_allocator_t *self, size_t size)
{
    return self->alloc(self, size);
}

/**
 * @brief Call the allocator's free method
 * @param self Allocator instance
 * @param ptr Pointer to the memory to free
 */
void cobalt_allocator_free(cobalt_allocator_t *self, void *ptr)
{
    self->free(self, ptr);
}

/**
 * @brief Call the allocator's realloc method
 * @param self Allocator instance
 * @param ptr Pointer to the memory to resize (may be NULL)
 * @param new_size New size in bytes
 * @return Pointer to the resized memory, or NULL on failure
 */
void *cobalt_allocator_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
{
    return self->realloc(self, ptr, new_size);
}
