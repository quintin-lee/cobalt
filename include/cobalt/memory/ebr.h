#ifndef COBALT_MEMORY_EBR_H
#define COBALT_MEMORY_EBR_H

/**
 * @file ebr.h
 * @brief Epoch-Based Reclamation (EBR) engine for lock-free data structures
 * @details Implements a 3-Epoch Safe Memory Reclamation (SMR) state machine.
 *          Provides wait-free registration, cacheline-aligned per-thread handles,
 *          and deferred batch reclamation to prevent Use-After-Free (UAF) and ABA hazards.
 *
 * @defgroup EBR Epoch-Based Reclamation
 * @ingroup Memory_Module
 * @{
 */

#include "cobalt/memory/allocator.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque structure representing the global EBR engine instance
 */
typedef struct cobalt_ebr cobalt_ebr_t;

/**
 * @brief Opaque per-thread handle representing a participant thread in the EBR engine
 */
typedef struct cobalt_ebr_handle cobalt_ebr_handle_t;

/**
 * @brief Create a new EBR engine instance
 * @param alloc Custom allocator instance (pass NULL to use the system default allocator)
 * @return Pointer to created cobalt_ebr_t instance, or NULL on allocation failure
 */
cobalt_ebr_t *cobalt_ebr_create(cobalt_allocator_t *alloc);

/**
 * @brief Destroy an EBR engine instance and free any remaining retired nodes
 * @param ebr Pointer to the EBR instance to destroy
 */
void cobalt_ebr_destroy(cobalt_ebr_t *ebr);

/**
 * @brief Register a thread with the EBR engine and obtain a per-thread handle
 * @param ebr Pointer to the EBR instance
 * @return Pointer to the allocated thread handle, or NULL on failure
 */
cobalt_ebr_handle_t *cobalt_ebr_register(cobalt_ebr_t *ebr);

/**
 * @brief Unregister a thread handle and flush its local retired nodes to the global collector
 * @param handle Pointer to the thread handle to unregister
 */
void cobalt_ebr_unregister(cobalt_ebr_handle_t *handle);

/**
 * @brief Enter an EBR critical read/write section
 * @param handle Pointer to the thread's EBR handle
 * @note Must be paired with a subsequent call to cobalt_ebr_exit().
 */
void cobalt_ebr_enter(cobalt_ebr_handle_t *handle);

/**
 * @brief Exit an EBR critical read/write section
 * @param handle Pointer to the thread's EBR handle
 */
void cobalt_ebr_exit(cobalt_ebr_handle_t *handle);

/**
 * @brief Retire a memory pointer for deferred safe reclamation
 * @param handle Pointer to the thread's EBR handle
 * @param ptr Pointer to memory that has been physically unlinked
 * @param free_fn Custom destructor function (pass NULL to use default free)
 * @param user_data Context pointer passed to free_fn
 */
void cobalt_ebr_retire(cobalt_ebr_handle_t *handle,
                       void                *ptr,
                       void (*free_fn)(void *ptr, void *user_data),
                       void *user_data);

/**
 * @brief Attempt to advance the global epoch and reclaim eligible retired nodes
 * @param ebr Pointer to the EBR instance
 * @return true if the global epoch was successfully advanced, false otherwise
 */
bool cobalt_ebr_try_advance(cobalt_ebr_t *ebr);

/**
 * @brief Synchronize and force epoch progression until all currently retired nodes are reclaimed
 * @param ebr Pointer to the EBR instance
 */
void cobalt_ebr_sync(cobalt_ebr_t *ebr);

/**
 * @brief Get unique instance ID of the EBR engine
 * @param ebr Pointer to the EBR instance
 * @return Monotonic unique 64-bit ID
 */
uint64_t cobalt_ebr_get_id(const cobalt_ebr_t *ebr);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* COBALT_MEMORY_EBR_H */
