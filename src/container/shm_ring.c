/**
 * @file shm_ring.c
 * @brief Lock-free shared memory ring buffer implementation (SPSC)
 */

#include "cobalt/container/shm_ring.h"
#include "cobalt/runtime/error.h"

#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#define cobalt_getpid() ((uint32_t)_getpid())
#else
#include <unistd.h>
#define cobalt_getpid() ((uint32_t)getpid())
#endif

#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

/**
 * @brief Internal SPSC control structure wrapping mapped memory.
 */
struct cobalt_shm_spsc {
    cobalt_shm_header_t *hdr;
    uint8_t             *buffer;
    size_t               capacity;
    size_t               mask;
    size_t               reserved_len;
    uint64_t             reserved_write_pos;
    bool                 is_reserved;
    size_t               peeked_frame_len;
    bool                 is_peeked;
};

int cobalt_shm_spsc_init(void               *shm_addr,
                         size_t              total_size,
                         size_t              capacity,
                         cobalt_shm_spsc_t **out_spsc)
{
    if (!shm_addr || !out_spsc || capacity == 0 || (capacity & (capacity - 1)) != 0 ||
        ((uintptr_t)shm_addr % 64) != 0) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    size_t required_size = sizeof(cobalt_shm_header_t) + capacity;
    if (total_size < required_size) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_shm_spsc_t *spsc = (cobalt_shm_spsc_t *)calloc(1, sizeof(cobalt_shm_spsc_t));
    if (!spsc) {
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    memset(shm_addr, 0, sizeof(cobalt_shm_header_t));
    cobalt_shm_header_t *hdr = (cobalt_shm_header_t *)shm_addr;
    hdr->magic               = COBALT_SHM_MAGIC;
    hdr->version             = COBALT_SHM_VERSION;
    hdr->mode                = COBALT_SHM_MODE_SPSC;
    hdr->total_shm_size      = (uint64_t)total_size;
    hdr->capacity            = (uint64_t)capacity;
    hdr->slot_size           = 0;
    hdr->creator_pid         = cobalt_getpid();

    atomic_init(&hdr->heartbeat, 0);
    atomic_init(&hdr->write_cursor, 0);
    atomic_init(&hdr->read_cursor, 0);
    atomic_init(&hdr->channel_state, 0);
    atomic_init(&hdr->waiting_readers, 0);
    atomic_init(&hdr->waiting_writers, 0);

    spsc->hdr                = hdr;
    spsc->buffer             = (uint8_t *)shm_addr + sizeof(cobalt_shm_header_t);
    spsc->capacity           = capacity;
    spsc->mask               = capacity - 1;
    spsc->reserved_len       = 0;
    spsc->reserved_write_pos = 0;
    spsc->is_reserved        = false;
    spsc->peeked_frame_len   = 0;
    spsc->is_peeked          = false;

    *out_spsc = spsc;
    return COBALT_SUCCESS;
}

int cobalt_shm_spsc_attach(void *shm_addr, size_t total_size, cobalt_shm_spsc_t **out_spsc)
{
    if (!shm_addr || !out_spsc || total_size < sizeof(cobalt_shm_header_t) ||
        ((uintptr_t)shm_addr % 64) != 0) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_shm_header_t *hdr = (cobalt_shm_header_t *)shm_addr;
    if (hdr->magic != COBALT_SHM_MAGIC || hdr->version != COBALT_SHM_VERSION ||
        hdr->mode != COBALT_SHM_MODE_SPSC) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    if (hdr->capacity == 0 || (hdr->capacity & (hdr->capacity - 1)) != 0) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    if (total_size < sizeof(cobalt_shm_header_t) + (size_t)hdr->capacity) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_shm_spsc_t *spsc = (cobalt_shm_spsc_t *)calloc(1, sizeof(cobalt_shm_spsc_t));
    if (!spsc) {
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    spsc->hdr                = hdr;
    spsc->buffer             = (uint8_t *)shm_addr + sizeof(cobalt_shm_header_t);
    spsc->capacity           = (size_t)hdr->capacity;
    spsc->mask               = (size_t)(hdr->capacity - 1);
    spsc->reserved_len       = 0;
    spsc->reserved_write_pos = 0;
    spsc->is_reserved        = false;
    spsc->peeked_frame_len   = 0;
    spsc->is_peeked          = false;

    *out_spsc = spsc;
    return COBALT_SUCCESS;
}

void cobalt_shm_spsc_detach(cobalt_shm_spsc_t *spsc)
{
    if (spsc) {
        free(spsc);
    }
}

int cobalt_shm_spsc_reserve(cobalt_shm_spsc_t *spsc, size_t len, void **out_ptr)
{
    if (!spsc || !out_ptr || spsc->is_reserved) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    if (len > UINT32_MAX - sizeof(cobalt_shm_frame_t)) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    size_t frame_size = ALIGN_UP(sizeof(cobalt_shm_frame_t) + len, COBALT_SHM_ALIGN);
    if (frame_size > spsc->capacity - sizeof(cobalt_shm_frame_t)) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_shm_header_t *hdr       = spsc->hdr;
    uint64_t             write_pos = atomic_load_explicit(&hdr->write_cursor, memory_order_relaxed);
    uint64_t             read_pos  = atomic_load_explicit(&hdr->read_cursor, memory_order_acquire);
    size_t               offset    = (size_t)(write_pos & spsc->mask);
    size_t               tail_space = spsc->capacity - offset;

    if (tail_space < frame_size) {
        uint64_t wrapped_write_pos = write_pos + tail_space;
        uint64_t used              = wrapped_write_pos - read_pos;
        if (used > spsc->capacity) {
            return COBALT_ERROR_QUEUE_FULL;
        }

        size_t free_space = spsc->capacity - (size_t)used;
        if (free_space < frame_size) {
            return COBALT_ERROR_QUEUE_FULL;
        }

        if (tail_space >= sizeof(cobalt_shm_frame_t)) {
            cobalt_shm_frame_t *wrap = (cobalt_shm_frame_t *)(spsc->buffer + offset);
            wrap->length             = (uint32_t)(tail_space - sizeof(cobalt_shm_frame_t));
            wrap->type               = 0;
            wrap->flags              = COBALT_SHM_FRAME_WRAP;
        }

        write_pos = wrapped_write_pos;
        offset    = 0;
    }

    uint64_t used = write_pos - read_pos;
    if (used > spsc->capacity || (spsc->capacity - (size_t)used) < frame_size) {
        return COBALT_ERROR_QUEUE_FULL;
    }

    *out_ptr                 = spsc->buffer + offset + sizeof(cobalt_shm_frame_t);
    spsc->reserved_len       = len;
    spsc->reserved_write_pos = write_pos;
    spsc->is_reserved        = true;
    return COBALT_SUCCESS;
}

int cobalt_shm_spsc_commit(cobalt_shm_spsc_t *spsc, uint16_t msg_type, size_t len)
{
    if (!spsc || !spsc->is_reserved || len > spsc->reserved_len) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    size_t              offset = (size_t)(spsc->reserved_write_pos & spsc->mask);
    cobalt_shm_frame_t *frame  = (cobalt_shm_frame_t *)(spsc->buffer + offset);
    frame->length              = (uint32_t)len;
    frame->type                = msg_type;
    frame->flags               = COBALT_SHM_FRAME_NORMAL;

    size_t frame_size = ALIGN_UP(sizeof(cobalt_shm_frame_t) + len, COBALT_SHM_ALIGN);
    atomic_store_explicit(
        &spsc->hdr->write_cursor, spsc->reserved_write_pos + frame_size, memory_order_release);
    spsc->is_reserved = false;
    return COBALT_SUCCESS;
}

int cobalt_shm_spsc_peek(cobalt_shm_spsc_t *spsc,
                         uint16_t          *out_type,
                         const void       **out_ptr,
                         size_t            *out_len)
{
    if (!spsc || !out_ptr) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    int wrap_count = 0;
    while (1) {
        uint64_t read_pos  = atomic_load_explicit(&spsc->hdr->read_cursor, memory_order_relaxed);
        uint64_t write_pos = atomic_load_explicit(&spsc->hdr->write_cursor, memory_order_acquire);

        if (read_pos == write_pos) {
            return COBALT_ERROR_EMPTY_CONTAINER;
        }
        if (read_pos > write_pos) {
            return COBALT_ERROR_GENERAL;
        }

        size_t              offset = (size_t)(read_pos & spsc->mask);
        cobalt_shm_frame_t *frame  = (cobalt_shm_frame_t *)(spsc->buffer + offset);

        if (frame->flags & COBALT_SHM_FRAME_WRAP) {
            if (offset == 0 || ++wrap_count > 1) {
                return COBALT_ERROR_GENERAL;
            }
            size_t tail_space = spsc->capacity - offset;
            atomic_store_explicit(
                &spsc->hdr->read_cursor, read_pos + tail_space, memory_order_release);
            continue;
        }

        uint64_t unread = write_pos - read_pos;
        size_t   frame_size =
            ALIGN_UP(sizeof(cobalt_shm_frame_t) + (size_t)frame->length, COBALT_SHM_ALIGN);
        if ((uint64_t)frame_size > unread || offset + frame_size > spsc->capacity) {
            return COBALT_ERROR_GENERAL;
        }

        if (out_type) {
            *out_type = frame->type;
        }
        *out_ptr = spsc->buffer + offset + sizeof(cobalt_shm_frame_t);
        if (out_len) {
            *out_len = (size_t)frame->length;
        }

        spsc->peeked_frame_len = frame_size;
        spsc->is_peeked        = true;
        return COBALT_SUCCESS;
    }
}

int cobalt_shm_spsc_consume(cobalt_shm_spsc_t *spsc)
{
    if (!spsc || !spsc->is_peeked) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    uint64_t read_pos = atomic_load_explicit(&spsc->hdr->read_cursor, memory_order_relaxed);
    atomic_store_explicit(
        &spsc->hdr->read_cursor, read_pos + spsc->peeked_frame_len, memory_order_release);
    spsc->is_peeked = false;
    return COBALT_SUCCESS;
}

int cobalt_shm_spsc_cancel(cobalt_shm_spsc_t *spsc)
{
    if (!spsc || !spsc->is_reserved) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    spsc->is_reserved = false;
    return COBALT_SUCCESS;
}

cobalt_shm_header_t *cobalt_shm_spsc_get_header(cobalt_shm_spsc_t *spsc)
{
    if (!spsc) {
        return NULL;
    }
    return spsc->hdr;
}

/**
 * @brief Internal MPMC control structure wrapping mapped memory.
 */
struct cobalt_shm_mpmc {
    cobalt_shm_header_t *hdr;
    uint8_t             *buffer;
    size_t               capacity;
    size_t               mask;
    size_t               slot_size;
    size_t               slot_stride; /* ALIGN_UP(sizeof(cobalt_shm_mpmc_slot_t) + slot_size, 64) */
};

int cobalt_shm_mpmc_init(void               *shm_addr,
                         size_t              total_size,
                         size_t              capacity,
                         size_t              slot_size,
                         cobalt_shm_mpmc_t **out_mpmc)
{
    if (!shm_addr || !out_mpmc || capacity < 2 || (capacity & (capacity - 1)) != 0 ||
        slot_size == 0 || slot_size > UINT32_MAX - sizeof(cobalt_shm_mpmc_slot_t) ||
        ((uintptr_t)shm_addr % 64) != 0) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    size_t slot_stride = ALIGN_UP(sizeof(cobalt_shm_mpmc_slot_t) + slot_size, 64);
    if (slot_stride == 0 || capacity > (SIZE_MAX - sizeof(cobalt_shm_header_t)) / slot_stride) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    size_t required_size = sizeof(cobalt_shm_header_t) + capacity * slot_stride;
    if (total_size < required_size) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_shm_mpmc_t *mpmc = (cobalt_shm_mpmc_t *)calloc(1, sizeof(cobalt_shm_mpmc_t));
    if (!mpmc) {
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    memset(shm_addr, 0, sizeof(cobalt_shm_header_t));
    cobalt_shm_header_t *hdr = (cobalt_shm_header_t *)shm_addr;
    hdr->magic               = COBALT_SHM_MAGIC;
    hdr->version             = COBALT_SHM_VERSION;
    hdr->mode                = COBALT_SHM_MODE_MPMC;
    hdr->total_shm_size      = (uint64_t)total_size;
    hdr->capacity            = (uint64_t)capacity;
    hdr->slot_size           = (uint32_t)slot_size;
    hdr->creator_pid         = cobalt_getpid();

    atomic_init(&hdr->heartbeat, 0);
    atomic_init(&hdr->write_cursor, 0);
    atomic_init(&hdr->read_cursor, 0);
    atomic_init(&hdr->channel_state, 0);
    atomic_init(&hdr->waiting_readers, 0);
    atomic_init(&hdr->waiting_writers, 0);

    uint8_t *buffer = (uint8_t *)shm_addr + sizeof(cobalt_shm_header_t);
    for (size_t i = 0; i < capacity; i++) {
        cobalt_shm_mpmc_slot_t *slot = (cobalt_shm_mpmc_slot_t *)(buffer + i * slot_stride);
        atomic_init(&slot->sequence, (uint64_t)i);
        slot->actual_len = 0;
        slot->reserved   = 0;
    }

    mpmc->hdr         = hdr;
    mpmc->buffer      = buffer;
    mpmc->capacity    = capacity;
    mpmc->mask        = capacity - 1;
    mpmc->slot_size   = slot_size;
    mpmc->slot_stride = slot_stride;

    *out_mpmc = mpmc;
    return COBALT_SUCCESS;
}

int cobalt_shm_mpmc_attach(void *shm_addr, size_t total_size, cobalt_shm_mpmc_t **out_mpmc)
{
    if (!shm_addr || !out_mpmc || total_size < sizeof(cobalt_shm_header_t) ||
        ((uintptr_t)shm_addr % 64) != 0) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_shm_header_t *hdr = (cobalt_shm_header_t *)shm_addr;
    if (hdr->magic != COBALT_SHM_MAGIC || hdr->version != COBALT_SHM_VERSION ||
        hdr->mode != COBALT_SHM_MODE_MPMC) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    if (hdr->capacity < 2 || (hdr->capacity & (hdr->capacity - 1)) != 0 || hdr->slot_size == 0) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    size_t slot_stride = ALIGN_UP(sizeof(cobalt_shm_mpmc_slot_t) + (size_t)hdr->slot_size, 64);
    if (slot_stride == 0 ||
        (size_t)hdr->capacity > (SIZE_MAX - sizeof(cobalt_shm_header_t)) / slot_stride) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    size_t required_size = sizeof(cobalt_shm_header_t) + (size_t)hdr->capacity * slot_stride;
    if (total_size < required_size) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_shm_mpmc_t *mpmc = (cobalt_shm_mpmc_t *)calloc(1, sizeof(cobalt_shm_mpmc_t));
    if (!mpmc) {
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    mpmc->hdr         = hdr;
    mpmc->buffer      = (uint8_t *)shm_addr + sizeof(cobalt_shm_header_t);
    mpmc->capacity    = (size_t)hdr->capacity;
    mpmc->mask        = (size_t)(hdr->capacity - 1);
    mpmc->slot_size   = (size_t)hdr->slot_size;
    mpmc->slot_stride = slot_stride;

    *out_mpmc = mpmc;
    return COBALT_SUCCESS;
}

void cobalt_shm_mpmc_detach(cobalt_shm_mpmc_t *mpmc)
{
    if (mpmc) {
        free(mpmc);
    }
}

int cobalt_shm_mpmc_push(cobalt_shm_mpmc_t *mpmc, const void *data, size_t len)
{
    if (!mpmc || !data || len > mpmc->slot_size) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    uint64_t pos = atomic_load_explicit(&mpmc->hdr->write_cursor, memory_order_relaxed);
    while (1) {
        cobalt_shm_mpmc_slot_t *slot =
            (cobalt_shm_mpmc_slot_t *)(mpmc->buffer + (pos & mpmc->mask) * mpmc->slot_stride);
        uint64_t seq  = atomic_load_explicit(&slot->sequence, memory_order_acquire);
        int64_t  diff = (int64_t)seq - (int64_t)pos;

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&mpmc->hdr->write_cursor,
                                                      &pos,
                                                      pos + 1,
                                                      memory_order_relaxed,
                                                      memory_order_relaxed)) {
                slot->actual_len = (uint32_t)len;
                memcpy(slot->payload, data, len);
                atomic_store_explicit(&slot->sequence, pos + 1, memory_order_release);
                return COBALT_SUCCESS;
            }
        } else if (diff < 0) {
            return COBALT_ERROR_QUEUE_FULL;
        } else {
            pos = atomic_load_explicit(&mpmc->hdr->write_cursor, memory_order_relaxed);
        }
    }
}

int cobalt_shm_mpmc_pop(cobalt_shm_mpmc_t *mpmc, void *buf, size_t buf_size, size_t *out_len)
{
    if (!mpmc || (!buf && buf_size > 0)) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    uint64_t pos = atomic_load_explicit(&mpmc->hdr->read_cursor, memory_order_relaxed);
    while (1) {
        cobalt_shm_mpmc_slot_t *slot =
            (cobalt_shm_mpmc_slot_t *)(mpmc->buffer + (pos & mpmc->mask) * mpmc->slot_stride);
        uint64_t seq  = atomic_load_explicit(&slot->sequence, memory_order_acquire);
        int64_t  diff = (int64_t)seq - (int64_t)(pos + 1);

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&mpmc->hdr->read_cursor,
                                                      &pos,
                                                      pos + 1,
                                                      memory_order_relaxed,
                                                      memory_order_relaxed)) {
                uint32_t actual = slot->actual_len;
                if (out_len) {
                    *out_len = (size_t)actual;
                }
                if (buf && buf_size > 0) {
                    size_t to_copy = (size_t)actual < buf_size ? (size_t)actual : buf_size;
                    memcpy(buf, slot->payload, to_copy);
                }
                atomic_store_explicit(&slot->sequence, pos + mpmc->capacity, memory_order_release);
                return COBALT_SUCCESS;
            }
        } else if (diff < 0) {
            return COBALT_ERROR_EMPTY_CONTAINER;
        } else {
            pos = atomic_load_explicit(&mpmc->hdr->read_cursor, memory_order_relaxed);
        }
    }
}

size_t cobalt_shm_mpmc_capacity(const cobalt_shm_mpmc_t *mpmc)
{
    if (!mpmc) {
        return 0;
    }
    return mpmc->capacity;
}

size_t cobalt_shm_mpmc_slot_size(const cobalt_shm_mpmc_t *mpmc)
{
    if (!mpmc) {
        return 0;
    }
    return mpmc->slot_size;
}

cobalt_shm_header_t *cobalt_shm_mpmc_get_header(cobalt_shm_mpmc_t *mpmc)
{
    if (!mpmc) {
        return NULL;
    }
    return mpmc->hdr;
}
