#ifndef MEMORY_ALLOCATOR_H
#define MEMORY_ALLOCATOR_H

/**
 * @file allocator.h
 * @brief Memory allocator interface
 * @details Provides an abstract set of function pointer interfaces for memory allocation,
 * deallocation, and reallocation to enable custom memory management strategies (such as using the
 * default system allocator, memory pools, etc.).
 *
 * @defgroup Memory_Module Memory module
 * @{
 */

/**
 * @defgroup Allocator Memory allocator module
 * @ingroup Memory_Module
 * @{
 */

#include <stddef.h>

/**
 * @brief Memory allocator virtual table interface
 * @details Contains function pointers for basic memory operations. Allows passing custom allocator
 * instances through object-oriented design.
 */
typedef struct cobalt_allocator {
    /**
     * @brief Function pointer for allocating memory
     * @param self Pointer to the allocator instance itself
     * @param size The requested byte size to allocate
     * @return void* Pointer to the newly allocated memory, or NULL on failure
     */
    void *(*alloc)(struct cobalt_allocator *self, size_t size);

    /**
     * @brief Function pointer for freeing memory
     * @param self Pointer to the allocator instance itself
     * @param ptr Pointer to the previously allocated memory
     */
    void (*free)(struct cobalt_allocator *self, void *ptr);

    /**
     * @brief Function pointer for reallocating memory
     * @param self Pointer to the allocator instance itself
     * @param ptr Pointer to the original memory
     * @param new_size The newly requested byte size
     * @return void* Pointer to the reallocated memory, or NULL on failure
     */
    void *(*realloc)(struct cobalt_allocator *self, void *ptr, size_t new_size);
} cobalt_allocator_t;

/**
 * @brief Get the default system memory allocator
 * @return cobalt_allocator_t* The system's default allocator instance (encapsulates
 * malloc/free/realloc)
 */
cobalt_allocator_t *cobalt_allocator_get_system(void);

/**
 * @brief Allocate memory using the specified allocator
 * @param self The allocator instance to use
 * @param size The size to allocate
 * @return void* Pointer to the newly allocated memory block
 */
void *cobalt_allocator_alloc(cobalt_allocator_t *self, size_t size);

/**
 * @brief Free memory using the specified allocator
 * @param self The allocator instance to use
 * @param ptr Pointer to the memory block to free
 */
void cobalt_allocator_free(cobalt_allocator_t *self, void *ptr);

/**
 * @brief Reallocate memory using the specified allocator
 * @param self The allocator instance to use
 * @param ptr Pointer to the original memory block
 * @param new_size The expected new size
 * @return void* Pointer to the reallocated memory block
 */
void *cobalt_allocator_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size);

/** @} */

/** @} */

#endif /* MEMORY_ALLOCATOR_H */
