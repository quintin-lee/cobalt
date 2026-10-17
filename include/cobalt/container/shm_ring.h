/**
 * @file shm_ring.h
 * @brief Lock-free shared memory ring buffer structures and SPSC protocol
 *
 * Provides cacheline-isolated header structures and single-producer single-consumer (SPSC)
 * variable-length zero-copy ring buffer operations over mapped shared memory.
 */

#ifndef COBALT_CONTAINER_SHM_RING_H
#define COBALT_CONTAINER_SHM_RING_H

#include "cobalt/runtime/error.h"

#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COBALT_SHM_MAGIC 0x434F425F53484D31ULL /* "COB_SHM1" */
#define COBALT_SHM_VERSION 1

typedef enum { COBALT_SHM_MODE_SPSC = 1, COBALT_SHM_MODE_MPMC = 2 } cobalt_shm_mode_t;

#define COBALT_SHM_FRAME_NORMAL 0x0000
#define COBALT_SHM_FRAME_WRAP 0x0001 /* Tail padding: reader wraps to offset 0 */

#define COBALT_SHM_ALIGN 8

/**
 * @brief Frame header preceding every message or wrap marker in the ring buffer.
 */
typedef struct {
    uint32_t length; /* Payload byte length */
    uint16_t type;   /* User-defined message type */
    uint16_t flags;  /* Frame flags (COBALT_SHM_FRAME_*) */
} cobalt_shm_frame_t;

/**
 * @brief Shared memory region header with cacheline-isolated cursors and metadata.
 */
typedef struct {
    /* Cache Line 0: Static Metadata */
    uint64_t magic;
    uint32_t version;
    uint32_t mode;
    uint64_t total_shm_size;
    uint64_t capacity; /* Must be a power of two */
    uint32_t slot_size;
    uint32_t creator_pid;

    /* Cache Line 1: Heartbeat & Reserved */
    _Alignas(64) atomic_uint_fast64_t heartbeat;
    uint8_t reserved[56];

    /* Cache Line 2: Producer Write Cursor (Isolated cache line) */
    _Alignas(64) atomic_uint_fast64_t write_cursor;

    /* Cache Line 3: Consumer Read Cursor (Isolated cache line) */
    _Alignas(64) atomic_uint_fast64_t read_cursor;

    /* Cache Line 4: Synchronization & Waiter Tracking */
    _Alignas(64) _Atomic uint32_t channel_state;
    _Atomic uint32_t waiting_readers;
    _Atomic uint32_t waiting_writers;
    uint8_t          state_padding[52];
} cobalt_shm_header_t;

/**
 * @brief Opaque handle for local SPSC ring buffer operations.
 */
typedef struct cobalt_shm_spsc cobalt_shm_spsc_t;

/**
 * @brief Initialize a new SPSC shared memory ring buffer header and structure.
 * @param shm_addr Base address of the mapped shared memory
 * @param total_size Total mapped size in bytes
 * @param capacity Buffer capacity in bytes (must be a power of two)
 * @param out_spsc Pointer to receive SPSC handle
 * @return COBALT_SUCCESS on success, error code otherwise
 */
int cobalt_shm_spsc_init(void               *shm_addr,
                         size_t              total_size,
                         size_t              capacity,
                         cobalt_shm_spsc_t **out_spsc);

/**
 * @brief Attach to an existing SPSC shared memory ring buffer.
 * @param shm_addr Base address of the mapped shared memory
 * @param total_size Total mapped size in bytes
 * @param out_spsc Pointer to receive SPSC handle
 * @return COBALT_SUCCESS on success, error code otherwise
 */
int cobalt_shm_spsc_attach(void *shm_addr, size_t total_size, cobalt_shm_spsc_t **out_spsc);

/**
 * @brief Detach and free the local SPSC handle (does not unmap shm).
 * @param spsc SPSC handle
 */
void cobalt_shm_spsc_detach(cobalt_shm_spsc_t *spsc);

/**
 * @brief Reserve contiguous space in the ring buffer for writing.
 * @param spsc SPSC handle
 * @param len Desired payload length in bytes
 * @param out_ptr Pointer to receive the direct pointer to write payload into
 * @return COBALT_SUCCESS on success, COBALT_ERROR_QUEUE_FULL if full, error code otherwise
 */
int cobalt_shm_spsc_reserve(cobalt_shm_spsc_t *spsc, size_t len, void **out_ptr);

/**
 * @brief Commit the previously reserved message, publishing it to the consumer.
 * @param spsc SPSC handle
 * @param msg_type User-defined message type code
 * @param len Actual payload length committed (must match or be <= reserved len)
 * @return COBALT_SUCCESS on success, error code otherwise
 */
int cobalt_shm_spsc_commit(cobalt_shm_spsc_t *spsc, uint16_t msg_type, size_t len);

/**
 * @brief Inspect the next available message in the ring buffer without consuming it.
 * @param spsc SPSC handle
 * @param out_type Pointer to receive message type
 * @param out_ptr Pointer to receive payload pointer
 * @param out_len Pointer to receive payload length
 * @return COBALT_SUCCESS on success, COBALT_ERROR_EMPTY_CONTAINER if empty
 */
int cobalt_shm_spsc_peek(cobalt_shm_spsc_t *spsc,
                         uint16_t          *out_type,
                         const void       **out_ptr,
                         size_t            *out_len);

/**
 * @brief Advance the consumer read cursor past the message previously inspected with peek.
 * @param spsc SPSC handle
 * @return COBALT_SUCCESS on success, error code otherwise
 */
int cobalt_shm_spsc_consume(cobalt_shm_spsc_t *spsc);

/**
 * @brief Get underlying header pointer.
 * @param spsc SPSC handle
 * @return Pointer to underlying shared memory header, or NULL if spsc is NULL
 */
cobalt_shm_header_t *cobalt_shm_spsc_get_header(cobalt_shm_spsc_t *spsc);

/**
 * @brief Cancel a pending reservation without committing any message.
 * @param spsc SPSC handle
 * @return COBALT_SUCCESS on success, error code otherwise
 */
int cobalt_shm_spsc_cancel(cobalt_shm_spsc_t *spsc);

/**
 * @brief MPMC slot header preceding payload within each aligned slot.
 */
typedef struct {
    atomic_uint_fast64_t sequence;   /* 8 bytes sequence number */
    uint32_t             actual_len; /* Actual payload bytes written */
    uint32_t             reserved;   /* Padding to 16 bytes */
    uint8_t              payload[];  /* Variable user payload up to slot_size */
} cobalt_shm_mpmc_slot_t;

/**
 * @brief Opaque handle for local MPMC ring buffer operations.
 */
typedef struct cobalt_shm_mpmc cobalt_shm_mpmc_t;

/**
 * @brief Initialize a new MPMC Dmitry Vyukov shared memory ring buffer.
 * @param shm_addr Base address of mapped shared memory (must be 64-byte aligned)
 * @param total_size Total mapped size in bytes
 * @param capacity Number of slots (must be a power of two)
 * @param slot_size Maximum payload capacity per slot in bytes (must be > 0)
 * @param out_mpmc Pointer to receive created MPMC handle
 * @return COBALT_SUCCESS on success, error code otherwise
 */
int cobalt_shm_mpmc_init(void               *shm_addr,
                         size_t              total_size,
                         size_t              capacity,
                         size_t              slot_size,
                         cobalt_shm_mpmc_t **out_mpmc);

/**
 * @brief Attach to an existing MPMC shared memory ring buffer.
 * @param shm_addr Base address of mapped shared memory (must be 64-byte aligned)
 * @param total_size Total mapped size in bytes
 * @param out_mpmc Pointer to receive MPMC handle
 * @return COBALT_SUCCESS on success, error code otherwise
 */
int cobalt_shm_mpmc_attach(void *shm_addr, size_t total_size, cobalt_shm_mpmc_t **out_mpmc);

/**
 * @brief Detach and free local MPMC handle (does not unmap shm).
 * @param mpmc MPMC handle
 */
void cobalt_shm_mpmc_detach(cobalt_shm_mpmc_t *mpmc);

/**
 * @brief Enqueue a message into the MPMC ring buffer (lock-free, multi-producer safe).
 * @param mpmc MPMC handle
 * @param data Pointer to message payload
 * @param len Length in bytes (must be <= slot_size)
 * @return COBALT_SUCCESS on success, COBALT_ERROR_QUEUE_FULL if ring is full
 */
int cobalt_shm_mpmc_push(cobalt_shm_mpmc_t *mpmc, const void *data, size_t len);

/**
 * @brief Dequeue a message from the MPMC ring buffer (lock-free, multi-consumer safe).
 * @param mpmc MPMC handle
 * @param buf Output buffer to copy payload into
 * @param buf_size Size of output buffer
 * @param out_len Pointer to receive actual payload length (optional, can be NULL)
 * @return COBALT_SUCCESS on success, COBALT_ERROR_EMPTY_CONTAINER if ring is empty
 */
int cobalt_shm_mpmc_pop(cobalt_shm_mpmc_t *mpmc, void *buf, size_t buf_size, size_t *out_len);

/**
 * @brief Get capacity (number of slots).
 * @param mpmc MPMC handle
 * @return Capacity in number of slots, or 0 if mpmc is NULL
 */
size_t cobalt_shm_mpmc_capacity(const cobalt_shm_mpmc_t *mpmc);

/**
 * @brief Get slot size (max bytes per slot).
 * @param mpmc MPMC handle
 * @return Maximum payload bytes per slot, or 0 if mpmc is NULL
 */
size_t cobalt_shm_mpmc_slot_size(const cobalt_shm_mpmc_t *mpmc);

/**
 * @brief Get underlying shared memory header.
 * @param mpmc MPMC handle
 * @return Pointer to underlying shared memory header, or NULL if mpmc is NULL
 */
cobalt_shm_header_t *cobalt_shm_mpmc_get_header(cobalt_shm_mpmc_t *mpmc);

#endif /* COBALT_CONTAINER_SHM_RING_H */
