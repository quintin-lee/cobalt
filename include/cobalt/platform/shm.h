#ifndef COBALT_PLATFORM_SHM_H
#define COBALT_PLATFORM_SHM_H

/**
 * @file shm.h
 * @brief Platform shared memory primitive
 * @details Provides cross-platform shared memory mapping abstractions.
 *          On POSIX uses shm_open/mmap; on Windows uses CreateFileMapping/MapViewOfFile.
 *
 * @defgroup PlatformShm Shared Memory Primitives
 * @ingroup Platform
 * @{
 */

#include <stddef.h>

typedef struct cobalt_shm cobalt_shm_t;

/**
 * @brief Create a new shared memory object and map it into the process address space.
 * @param name Name of the shared memory object (e.g., "/cobalt_test_shm")
 * @param size Desired size in bytes (must be > 0)
 * @param out_shm Pointer to store the created shared memory handle
 * @return COBALT_SUCCESS on success, error code otherwise
 */
int cobalt_shm_create(const char *name, size_t size, cobalt_shm_t **out_shm);

/**
 * @brief Open and map an existing shared memory object.
 * @param name Name of the shared memory object
 * @param out_shm Pointer to store the opened shared memory handle
 * @return COBALT_SUCCESS on success, error code otherwise
 */
int cobalt_shm_open(const char *name, cobalt_shm_t **out_shm);

/**
 * @brief Get the mapped virtual address of the shared memory object.
 */
void *cobalt_shm_get_addr(cobalt_shm_t *shm);

/**
 * @brief Get the size in bytes of the mapped shared memory object.
 */
size_t cobalt_shm_get_size(const cobalt_shm_t *shm);

/**
 * @brief Unmap and close the shared memory object handle.
 * @note This does NOT unlink/delete the shared memory object from the OS namespace.
 */
void cobalt_shm_close(cobalt_shm_t *shm);

/**
 * @brief Unlink (delete) the shared memory object from the OS namespace.
 */
int cobalt_shm_unlink(const char *name);

/** @} */

#endif /* COBALT_PLATFORM_SHM_H */
