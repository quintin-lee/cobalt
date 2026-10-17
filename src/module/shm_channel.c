/**
 * @file shm_channel.c
 * @brief Layer 2 Shared memory channel facade, wait policies and EventLoop integration
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "cobalt/module/shm_channel.h"
#include "cobalt/container/shm_ring.h"
#include "cobalt/module/eventloop.h"
#include "cobalt/platform/notifier.h"
#include "cobalt/platform/shm.h"
#include "cobalt/platform/thread.h"
#include "cobalt/runtime/error.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef POLLIN
#define POLLIN 0x0001
#endif
#ifndef COBALT_EVENT_READ
#define COBALT_EVENT_READ POLLIN
#endif

#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

struct cobalt_shm_channel {
    char                    *name;
    bool                     is_creator;
    cobalt_shm_mode_t        mode;
    cobalt_shm_wait_policy_t wait_policy;
    uint32_t                 spin_threshold;

    cobalt_shm_t        *shm;
    cobalt_notifier_t   *notif;
    cobalt_shm_spsc_t   *spsc;
    cobalt_shm_mpmc_t   *mpmc;
    cobalt_shm_header_t *hdr;

    cobalt_eventloop_t *loop;
    cobalt_shm_msg_cb   cb;
    void               *udata;
    bool                attached_to_loop;

    void  *drain_buf;
    size_t drain_buf_size;
};

static uint64_t get_time_monotonic_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}

static inline void cpu_pause(void)
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(__GNUC__) || defined(__clang__)
    __builtin_ia32_pause();
#elif defined(_MSC_VER)
    _mm_pause();
#endif
#elif defined(__aarch64__)
#if defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("yield" ::: "memory");
#endif
#endif
}

static inline void notify_readers_if_waiting(cobalt_shm_channel_t *chan)
{
    if (chan->wait_policy != COBALT_SHM_WAIT_SPIN && chan->notif != NULL && chan->hdr != NULL) {
        if (atomic_load_explicit(&chan->hdr->waiting_readers, memory_order_relaxed) > 0) {
            cobalt_notifier_notify(chan->notif);
        }
    }
}

static inline void notify_writers_if_waiting(cobalt_shm_channel_t *chan)
{
    if (chan->wait_policy != COBALT_SHM_WAIT_SPIN && chan->notif != NULL && chan->hdr != NULL) {
        if (atomic_load_explicit(&chan->hdr->waiting_writers, memory_order_relaxed) > 0) {
            cobalt_notifier_notify(chan->notif);
        }
    }
}

static int
try_send_spsc(cobalt_shm_channel_t *chan, uint16_t msg_type, const void *data, size_t len)
{
    void *ptr = NULL;
    int   rc  = cobalt_shm_spsc_reserve(chan->spsc, len, &ptr);
    if (rc != COBALT_SUCCESS) {
        return rc;
    }
    if (len > 0 && data != NULL) {
        memcpy(ptr, data, len);
    }
    rc = cobalt_shm_spsc_commit(chan->spsc, msg_type, len);
    if (rc == COBALT_SUCCESS) {
        notify_readers_if_waiting(chan);
    }
    return rc;
}

static int try_send_mpmc(cobalt_shm_channel_t *chan, const void *data, size_t len)
{
    int rc = cobalt_shm_mpmc_push(chan->mpmc, data, len);
    if (rc == COBALT_SUCCESS) {
        notify_readers_if_waiting(chan);
    }
    return rc;
}

static int try_send(cobalt_shm_channel_t *chan, uint16_t msg_type, const void *data, size_t len)
{
    if (chan->mode == COBALT_SHM_MODE_SPSC) {
        return try_send_spsc(chan, msg_type, data, len);
    } else if (chan->mode == COBALT_SHM_MODE_MPMC) {
        return try_send_mpmc(chan, data, len);
    }
    return COBALT_ERROR_INVALID_ARGUMENT;
}

static int try_recv_spsc(
    cobalt_shm_channel_t *chan, uint16_t *out_type, void *buf, size_t buf_size, size_t *out_len)
{
    uint16_t    type    = 0;
    const void *payload = NULL;
    size_t      len     = 0;
    int         rc      = cobalt_shm_spsc_peek(chan->spsc, &type, &payload, &len);
    if (rc != COBALT_SUCCESS) {
        return rc;
    }
    if (out_type) {
        *out_type = type;
    }
    if (out_len) {
        *out_len = len;
    }
    if (buf && buf_size > 0 && len > 0) {
        size_t to_copy = len < buf_size ? len : buf_size;
        memcpy(buf, payload, to_copy);
    }
    rc = cobalt_shm_spsc_consume(chan->spsc);
    if (rc == COBALT_SUCCESS) {
        notify_writers_if_waiting(chan);
    }
    return rc;
}

static int try_recv_mpmc(
    cobalt_shm_channel_t *chan, uint16_t *out_type, void *buf, size_t buf_size, size_t *out_len)
{
    if (out_type) {
        *out_type = 0;
    }
    int rc = cobalt_shm_mpmc_pop(chan->mpmc, buf, buf_size, out_len);
    if (rc == COBALT_SUCCESS) {
        notify_writers_if_waiting(chan);
    }
    return rc;
}

static int try_recv(
    cobalt_shm_channel_t *chan, uint16_t *out_type, void *buf, size_t buf_size, size_t *out_len)
{
    if (chan->mode == COBALT_SHM_MODE_SPSC) {
        return try_recv_spsc(chan, out_type, buf, buf_size, out_len);
    } else if (chan->mode == COBALT_SHM_MODE_MPMC) {
        return try_recv_mpmc(chan, out_type, buf, buf_size, out_len);
    }
    return COBALT_ERROR_INVALID_ARGUMENT;
}

cobalt_shm_channel_t *cobalt_shm_channel_create(const cobalt_shm_config_t *config)
{
    if (!config || !config->name) {
        return NULL;
    }
    if (config->mode != COBALT_SHM_MODE_SPSC && config->mode != COBALT_SHM_MODE_MPMC) {
        return NULL;
    }
    if (config->mode == COBALT_SHM_MODE_SPSC) {
        if (config->capacity == 0 || (config->capacity & (config->capacity - 1)) != 0) {
            return NULL;
        }
    } else {
        if (config->capacity < 2 || (config->capacity & (config->capacity - 1)) != 0 ||
            config->slot_size == 0) {
            return NULL;
        }
    }

    size_t required_size = 0;
    if (config->mode == COBALT_SHM_MODE_SPSC) {
        required_size = sizeof(cobalt_shm_header_t) + config->capacity;
    } else {
        size_t slot_stride = ALIGN_UP(sizeof(cobalt_shm_mpmc_slot_t) + config->slot_size, 64);
        if (slot_stride == 0 ||
            config->capacity > (SIZE_MAX - sizeof(cobalt_shm_header_t)) / slot_stride) {
            return NULL;
        }
        required_size = sizeof(cobalt_shm_header_t) + config->capacity * slot_stride;
    }

    char notif_name[256];
    int  written = snprintf(notif_name, sizeof(notif_name), "%s_notif", config->name);
    if (written < 0 || (size_t)written >= sizeof(notif_name)) {
        return NULL;
    }

    cobalt_shm_t      *shm        = NULL;
    cobalt_notifier_t *notif      = NULL;
    bool               is_creator = false;
    int                rc;

    if (config->create_if_missing) {
        rc = cobalt_shm_create(config->name, required_size, &shm);
        if (rc == COBALT_SUCCESS) {
            is_creator = true;
            rc         = cobalt_notifier_create(notif_name, &notif);
            if (rc == COBALT_ERROR_ALREADY_EXISTS) {
                rc = cobalt_notifier_open(notif_name, &notif);
            }
            if (rc != COBALT_SUCCESS) {
                cobalt_shm_close(shm);
                cobalt_shm_unlink(config->name);
                return NULL;
            }
        } else if (rc == COBALT_ERROR_ALREADY_EXISTS) {
            rc = cobalt_shm_open(config->name, &shm);
            if (rc != COBALT_SUCCESS) {
                return NULL;
            }
            is_creator = false;
            rc         = cobalt_notifier_open(notif_name, &notif);
            if (rc != COBALT_SUCCESS) {
                cobalt_notifier_create(notif_name, &notif);
            }
        } else {
            return NULL;
        }
    } else {
        rc = cobalt_shm_open(config->name, &shm);
        if (rc != COBALT_SUCCESS) {
            return NULL;
        }
        is_creator = false;
        rc         = cobalt_notifier_open(notif_name, &notif);
        if (rc != COBALT_SUCCESS) {
            cobalt_notifier_create(notif_name, &notif);
        }
    }

    void  *addr       = cobalt_shm_get_addr(shm);
    size_t total_size = cobalt_shm_get_size(shm);
    if (!addr) {
        if (notif) {
            cobalt_notifier_destroy(notif);
        }
        cobalt_shm_close(shm);
        if (is_creator) {
            cobalt_shm_unlink(config->name);
        }
        return NULL;
    }

    cobalt_shm_spsc_t   *spsc = NULL;
    cobalt_shm_mpmc_t   *mpmc = NULL;
    cobalt_shm_header_t *hdr  = NULL;

    if (is_creator) {
        if (config->mode == COBALT_SHM_MODE_SPSC) {
            rc = cobalt_shm_spsc_init(addr, total_size, config->capacity, &spsc);
            if (rc == COBALT_SUCCESS) {
                hdr = cobalt_shm_spsc_get_header(spsc);
            }
        } else {
            rc = cobalt_shm_mpmc_init(addr, total_size, config->capacity, config->slot_size, &mpmc);
            if (rc == COBALT_SUCCESS) {
                hdr = cobalt_shm_mpmc_get_header(mpmc);
            }
        }
    } else {
        if (config->mode == COBALT_SHM_MODE_SPSC) {
            rc = cobalt_shm_spsc_attach(addr, total_size, &spsc);
            if (rc == COBALT_SUCCESS) {
                hdr = cobalt_shm_spsc_get_header(spsc);
            }
        } else {
            rc = cobalt_shm_mpmc_attach(addr, total_size, &mpmc);
            if (rc == COBALT_SUCCESS) {
                hdr = cobalt_shm_mpmc_get_header(mpmc);
            }
        }
    }

    if (rc != COBALT_SUCCESS) {
        if (notif) {
            cobalt_notifier_destroy(notif);
        }
        cobalt_shm_close(shm);
        if (is_creator) {
            cobalt_shm_unlink(config->name);
        }
        return NULL;
    }

    cobalt_shm_channel_t *chan = (cobalt_shm_channel_t *)calloc(1, sizeof(cobalt_shm_channel_t));
    if (!chan) {
        if (spsc) {
            cobalt_shm_spsc_detach(spsc);
        }
        if (mpmc) {
            cobalt_shm_mpmc_detach(mpmc);
        }
        if (notif) {
            cobalt_notifier_destroy(notif);
        }
        cobalt_shm_close(shm);
        if (is_creator) {
            cobalt_shm_unlink(config->name);
        }
        return NULL;
    }

    chan->name           = strdup(config->name);
    chan->is_creator     = is_creator;
    chan->mode           = config->mode;
    chan->wait_policy    = config->wait_policy;
    chan->spin_threshold = config->spin_threshold > 0 ? config->spin_threshold : 1000;
    chan->shm            = shm;
    chan->notif          = notif;
    chan->spsc           = spsc;
    chan->mpmc           = mpmc;
    chan->hdr            = hdr;

    return chan;
}

cobalt_shm_channel_t *cobalt_shm_channel_open(const char *name, cobalt_shm_wait_policy_t policy)
{
    if (!name) {
        return NULL;
    }

    cobalt_shm_t *shm = NULL;
    int           rc  = cobalt_shm_open(name, &shm);
    if (rc != COBALT_SUCCESS) {
        return NULL;
    }

    void  *addr       = cobalt_shm_get_addr(shm);
    size_t total_size = cobalt_shm_get_size(shm);
    if (!addr || total_size < sizeof(cobalt_shm_header_t)) {
        cobalt_shm_close(shm);
        return NULL;
    }

    cobalt_shm_header_t *hdr = (cobalt_shm_header_t *)addr;
    if (hdr->magic != COBALT_SHM_MAGIC || hdr->version != COBALT_SHM_VERSION) {
        cobalt_shm_close(shm);
        return NULL;
    }

    char notif_name[256];
    int  written = snprintf(notif_name, sizeof(notif_name), "%s_notif", name);
    if (written < 0 || (size_t)written >= sizeof(notif_name)) {
        cobalt_shm_close(shm);
        return NULL;
    }

    cobalt_notifier_t *notif = NULL;
    cobalt_notifier_open(notif_name, &notif);

    cobalt_shm_spsc_t *spsc = NULL;
    cobalt_shm_mpmc_t *mpmc = NULL;

    if (hdr->mode == COBALT_SHM_MODE_SPSC) {
        rc = cobalt_shm_spsc_attach(addr, total_size, &spsc);
        if (rc == COBALT_SUCCESS) {
            hdr = cobalt_shm_spsc_get_header(spsc);
        }
    } else if (hdr->mode == COBALT_SHM_MODE_MPMC) {
        rc = cobalt_shm_mpmc_attach(addr, total_size, &mpmc);
        if (rc == COBALT_SUCCESS) {
            hdr = cobalt_shm_mpmc_get_header(mpmc);
        }
    } else {
        rc = COBALT_ERROR_INVALID_ARGUMENT;
    }

    if (rc != COBALT_SUCCESS) {
        if (notif) {
            cobalt_notifier_destroy(notif);
        }
        cobalt_shm_close(shm);
        return NULL;
    }

    cobalt_shm_channel_t *chan = (cobalt_shm_channel_t *)calloc(1, sizeof(cobalt_shm_channel_t));
    if (!chan) {
        if (spsc) {
            cobalt_shm_spsc_detach(spsc);
        }
        if (mpmc) {
            cobalt_shm_mpmc_detach(mpmc);
        }
        if (notif) {
            cobalt_notifier_destroy(notif);
        }
        cobalt_shm_close(shm);
        return NULL;
    }

    chan->name           = strdup(name);
    chan->is_creator     = false;
    chan->mode           = (cobalt_shm_mode_t)hdr->mode;
    chan->wait_policy    = policy;
    chan->spin_threshold = 1000;
    chan->shm            = shm;
    chan->notif          = notif;
    chan->spsc           = spsc;
    chan->mpmc           = mpmc;
    chan->hdr            = hdr;

    return chan;
}

void cobalt_shm_channel_destroy(cobalt_shm_channel_t *chan)
{
    if (!chan) {
        return;
    }
    if (chan->attached_to_loop) {
        cobalt_shm_channel_detach_eventloop(chan);
    }
    if (chan->spsc) {
        cobalt_shm_spsc_detach(chan->spsc);
        chan->spsc = NULL;
    }
    if (chan->mpmc) {
        cobalt_shm_mpmc_detach(chan->mpmc);
        chan->mpmc = NULL;
    }
    if (chan->notif) {
        cobalt_notifier_destroy(chan->notif);
        chan->notif = NULL;
    }
    if (chan->shm) {
        cobalt_shm_close(chan->shm);
        chan->shm = NULL;
    }
    if (chan->is_creator && chan->name) {
        cobalt_shm_unlink(chan->name);
    }
    if (chan->name) {
        free(chan->name);
        chan->name = NULL;
    }
    if (chan->drain_buf) {
        free(chan->drain_buf);
        chan->drain_buf = NULL;
    }
    free(chan);
}

int cobalt_shm_channel_reserve(cobalt_shm_channel_t *chan, size_t len, void **out_ptr)
{
    if (!chan || chan->mode != COBALT_SHM_MODE_SPSC || !chan->spsc) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    return cobalt_shm_spsc_reserve(chan->spsc, len, out_ptr);
}

int cobalt_shm_channel_commit(cobalt_shm_channel_t *chan, uint16_t msg_type, size_t len)
{
    if (!chan || chan->mode != COBALT_SHM_MODE_SPSC || !chan->spsc) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    int rc = cobalt_shm_spsc_commit(chan->spsc, msg_type, len);
    if (rc == COBALT_SUCCESS) {
        notify_readers_if_waiting(chan);
    }
    return rc;
}

int cobalt_shm_channel_cancel(cobalt_shm_channel_t *chan)
{
    if (!chan || chan->mode != COBALT_SHM_MODE_SPSC || !chan->spsc) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    return cobalt_shm_spsc_cancel(chan->spsc);
}

int cobalt_shm_channel_peek(cobalt_shm_channel_t *chan,
                            uint16_t             *out_type,
                            const void          **out_ptr,
                            size_t               *out_len)
{
    if (!chan || chan->mode != COBALT_SHM_MODE_SPSC || !chan->spsc) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    return cobalt_shm_spsc_peek(chan->spsc, out_type, out_ptr, out_len);
}

int cobalt_shm_channel_consume(cobalt_shm_channel_t *chan)
{
    if (!chan || chan->mode != COBALT_SHM_MODE_SPSC || !chan->spsc) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    int rc = cobalt_shm_spsc_consume(chan->spsc);
    if (rc == COBALT_SUCCESS) {
        notify_writers_if_waiting(chan);
    }
    return rc;
}

int cobalt_shm_channel_send(
    cobalt_shm_channel_t *chan, uint16_t msg_type, const void *data, size_t len, int timeout_ms)
{
    if (!chan || (!data && len > 0)) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    int rc = try_send(chan, msg_type, data, len);
    if (rc != COBALT_ERROR_QUEUE_FULL) {
        return rc;
    }
    if (timeout_ms == 0) {
        return COBALT_ERROR_QUEUE_FULL;
    }

    uint64_t start_ms = 0;
    if (timeout_ms > 0) {
        start_ms = get_time_monotonic_ms();
    }

    /* Wait policy handling */
    if (chan->wait_policy == COBALT_SHM_WAIT_SPIN) {
        while (1) {
            if (timeout_ms > 0) {
                uint64_t now_ms = get_time_monotonic_ms();
                if (now_ms - start_ms >= (uint64_t)timeout_ms) {
                    return COBALT_ERROR_TIMEOUT;
                }
            }
            rc = try_send(chan, msg_type, data, len);
            if (rc != COBALT_ERROR_QUEUE_FULL) {
                return rc;
            }
            cpu_pause();
        }
    } else if (chan->wait_policy == COBALT_SHM_WAIT_ADAPTIVE) {
        uint32_t threshold = chan->spin_threshold > 0 ? chan->spin_threshold : 1000;
        for (uint32_t i = 0; i < threshold; i++) {
            if (timeout_ms > 0) {
                uint64_t now_ms = get_time_monotonic_ms();
                if (now_ms - start_ms >= (uint64_t)timeout_ms) {
                    return COBALT_ERROR_TIMEOUT;
                }
            }
            rc = try_send(chan, msg_type, data, len);
            if (rc != COBALT_ERROR_QUEUE_FULL) {
                return rc;
            }
            cpu_pause();
        }
        /* Fall through to BLOCKING */
    }

    /* BLOCKING policy (or ADAPTIVE threshold exceeded) */
    while (1) {
        int remaining_ms = -1;
        if (timeout_ms > 0) {
            uint64_t now_ms  = get_time_monotonic_ms();
            uint64_t elapsed = now_ms - start_ms;
            if (elapsed >= (uint64_t)timeout_ms) {
                return COBALT_ERROR_TIMEOUT;
            }
            remaining_ms = (int)((uint64_t)timeout_ms - elapsed);
        }

        if (chan->hdr) {
            atomic_fetch_add_explicit(&chan->hdr->waiting_writers, 1, memory_order_relaxed);
        }

        /* Recheck data before sleep */
        rc = try_send(chan, msg_type, data, len);
        if (rc != COBALT_ERROR_QUEUE_FULL) {
            if (chan->hdr) {
                atomic_fetch_sub_explicit(&chan->hdr->waiting_writers, 1, memory_order_relaxed);
            }
            return rc;
        }

        int wait_rc = COBALT_SUCCESS;
        if (chan->notif != NULL) {
            wait_rc = cobalt_notifier_wait(chan->notif, remaining_ms);
        } else {
            cobalt_thread_yield();
        }

        if (chan->hdr) {
            atomic_fetch_sub_explicit(&chan->hdr->waiting_writers, 1, memory_order_relaxed);
        }

        if (wait_rc == COBALT_ERROR_TIMEOUT) {
            return COBALT_ERROR_TIMEOUT;
        }

        rc = try_send(chan, msg_type, data, len);
        if (rc != COBALT_ERROR_QUEUE_FULL) {
            return rc;
        }
    }
}

int cobalt_shm_channel_recv(cobalt_shm_channel_t *chan,
                            uint16_t             *out_type,
                            void                 *buf,
                            size_t                buf_size,
                            size_t               *out_len,
                            int                   timeout_ms)
{
    if (!chan || (!buf && buf_size > 0)) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    int rc = try_recv(chan, out_type, buf, buf_size, out_len);
    if (rc != COBALT_ERROR_EMPTY_CONTAINER) {
        return rc;
    }
    if (timeout_ms == 0) {
        return COBALT_ERROR_EMPTY_CONTAINER;
    }

    uint64_t start_ms = 0;
    if (timeout_ms > 0) {
        start_ms = get_time_monotonic_ms();
    }

    /* Wait policy handling */
    if (chan->wait_policy == COBALT_SHM_WAIT_SPIN) {
        while (1) {
            if (timeout_ms > 0) {
                uint64_t now_ms = get_time_monotonic_ms();
                if (now_ms - start_ms >= (uint64_t)timeout_ms) {
                    return COBALT_ERROR_TIMEOUT;
                }
            }
            rc = try_recv(chan, out_type, buf, buf_size, out_len);
            if (rc != COBALT_ERROR_EMPTY_CONTAINER) {
                return rc;
            }
            cpu_pause();
        }
    } else if (chan->wait_policy == COBALT_SHM_WAIT_ADAPTIVE) {
        uint32_t threshold = chan->spin_threshold > 0 ? chan->spin_threshold : 1000;
        for (uint32_t i = 0; i < threshold; i++) {
            if (timeout_ms > 0) {
                uint64_t now_ms = get_time_monotonic_ms();
                if (now_ms - start_ms >= (uint64_t)timeout_ms) {
                    return COBALT_ERROR_TIMEOUT;
                }
            }
            rc = try_recv(chan, out_type, buf, buf_size, out_len);
            if (rc != COBALT_ERROR_EMPTY_CONTAINER) {
                return rc;
            }
            cpu_pause();
        }
        /* Fall through to BLOCKING */
    }

    /* BLOCKING policy (or ADAPTIVE threshold exceeded) */
    while (1) {
        int remaining_ms = -1;
        if (timeout_ms > 0) {
            uint64_t now_ms  = get_time_monotonic_ms();
            uint64_t elapsed = now_ms - start_ms;
            if (elapsed >= (uint64_t)timeout_ms) {
                return COBALT_ERROR_TIMEOUT;
            }
            remaining_ms = (int)((uint64_t)timeout_ms - elapsed);
        }

        if (chan->hdr) {
            atomic_fetch_add_explicit(&chan->hdr->waiting_readers, 1, memory_order_relaxed);
        }

        /* Recheck data before sleep */
        rc = try_recv(chan, out_type, buf, buf_size, out_len);
        if (rc != COBALT_ERROR_EMPTY_CONTAINER) {
            if (chan->hdr) {
                atomic_fetch_sub_explicit(&chan->hdr->waiting_readers, 1, memory_order_relaxed);
            }
            return rc;
        }

        int wait_rc = COBALT_SUCCESS;
        if (chan->notif != NULL) {
            wait_rc = cobalt_notifier_wait(chan->notif, remaining_ms);
        } else {
            cobalt_thread_yield();
        }

        if (chan->hdr) {
            atomic_fetch_sub_explicit(&chan->hdr->waiting_readers, 1, memory_order_relaxed);
        }

        if (wait_rc == COBALT_ERROR_TIMEOUT) {
            return COBALT_ERROR_TIMEOUT;
        }

        rc = try_recv(chan, out_type, buf, buf_size, out_len);
        if (rc != COBALT_ERROR_EMPTY_CONTAINER) {
            return rc;
        }
    }
}

cobalt_shm_mode_t cobalt_shm_channel_get_mode(const cobalt_shm_channel_t *chan)
{
    if (!chan) {
        return (cobalt_shm_mode_t)0;
    }
    return chan->mode;
}

cobalt_shm_wait_policy_t cobalt_shm_channel_get_wait_policy(const cobalt_shm_channel_t *chan)
{
    if (!chan) {
        return (cobalt_shm_wait_policy_t)0;
    }
    return chan->wait_policy;
}

int cobalt_shm_channel_get_read_fd(const cobalt_shm_channel_t *chan)
{
    if (!chan || !chan->notif) {
        return -1;
    }
    return cobalt_notifier_get_fd(chan->notif);
}

static void shm_channel_eventloop_cb(cobalt_fd_t fd, cobalt_events_t events, void *user_data)
{
    (void)fd;
    (void)events;
    cobalt_shm_channel_t *chan = (cobalt_shm_channel_t *)user_data;
    if (!chan) {
        return;
    }

    /* Drain notifier descriptor */
    if (chan->notif) {
        cobalt_notifier_wait(chan->notif, 0);
    }

    if (!chan->cb) {
        return;
    }

    /* Batch drain all available messages */
    if (chan->mode == COBALT_SHM_MODE_SPSC && chan->spsc) {
        while (chan->attached_to_loop) {
            uint16_t    type    = 0;
            const void *payload = NULL;
            size_t      len     = 0;
            int         rc      = cobalt_shm_spsc_peek(chan->spsc, &type, &payload, &len);
            if (rc != COBALT_SUCCESS) {
                break;
            }
            chan->cb(chan, type, payload, len, chan->udata);
            cobalt_shm_spsc_consume(chan->spsc);
            notify_writers_if_waiting(chan);
        }
    } else if (chan->mode == COBALT_SHM_MODE_MPMC && chan->mpmc) {
        if (chan->drain_buf && chan->drain_buf_size > 0) {
            while (chan->attached_to_loop) {
                size_t len = 0;
                int    rc =
                    cobalt_shm_mpmc_pop(chan->mpmc, chan->drain_buf, chan->drain_buf_size, &len);
                if (rc != COBALT_SUCCESS) {
                    break;
                }
                chan->cb(chan, 0, chan->drain_buf, len, chan->udata);
                notify_writers_if_waiting(chan);
            }
        }
    }
}

int cobalt_shm_channel_attach_eventloop(cobalt_shm_channel_t *chan,
                                        cobalt_eventloop_t   *loop,
                                        cobalt_shm_msg_cb     cb,
                                        void                 *user_data)
{
    if (!chan || !loop || !cb) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    if (chan->attached_to_loop) {
        return COBALT_ERROR_ALREADY_EXISTS;
    }
    int fd = cobalt_shm_channel_get_read_fd(chan);
    if (fd < 0) {
        return COBALT_ERROR_GENERAL;
    }

    if (chan->mode == COBALT_SHM_MODE_MPMC) {
        size_t slot_sz = cobalt_shm_mpmc_slot_size(chan->mpmc);
        if (slot_sz == 0) {
            slot_sz = 4096;
        }
        chan->drain_buf = malloc(slot_sz);
        if (!chan->drain_buf) {
            return COBALT_ERROR_OUT_OF_MEMORY;
        }
        chan->drain_buf_size = slot_sz;
    }

    chan->loop             = loop;
    chan->cb               = cb;
    chan->udata            = user_data;
    chan->attached_to_loop = true;

    /* Increment waiting_readers while managed by EventLoop so peer always notifies on write */
    if (chan->hdr) {
        atomic_fetch_add_explicit(&chan->hdr->waiting_readers, 1, memory_order_relaxed);
    }

    int rc = cobalt_eventloop_add_fd(loop, fd, COBALT_EVENT_READ, shm_channel_eventloop_cb, chan);
    if (rc != 0) {
        if (chan->hdr) {
            atomic_fetch_sub_explicit(&chan->hdr->waiting_readers, 1, memory_order_relaxed);
        }
        chan->loop             = NULL;
        chan->cb               = NULL;
        chan->udata            = NULL;
        chan->attached_to_loop = false;
        if (chan->drain_buf) {
            free(chan->drain_buf);
            chan->drain_buf      = NULL;
            chan->drain_buf_size = 0;
        }
        return COBALT_ERROR_GENERAL;
    }

    /* If messages are already present, notify to trigger initial read */
    if (chan->notif) {
        if (chan->mode == COBALT_SHM_MODE_SPSC && chan->spsc) {
            uint16_t    t;
            const void *p;
            size_t      l;
            if (cobalt_shm_spsc_peek(chan->spsc, &t, &p, &l) == COBALT_SUCCESS) {
                cobalt_notifier_notify(chan->notif);
            }
        }
    }

    return COBALT_SUCCESS;
}

int cobalt_shm_channel_detach_eventloop(cobalt_shm_channel_t *chan)
{
    if (!chan) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    if (!chan->attached_to_loop) {
        return COBALT_SUCCESS;
    }
    int fd = cobalt_shm_channel_get_read_fd(chan);
    if (fd >= 0 && chan->loop) {
        cobalt_eventloop_del_fd(chan->loop, fd);
    }
    if (chan->hdr) {
        atomic_fetch_sub_explicit(&chan->hdr->waiting_readers, 1, memory_order_relaxed);
    }
    chan->loop             = NULL;
    chan->cb               = NULL;
    chan->udata            = NULL;
    chan->attached_to_loop = false;
    if (chan->drain_buf) {
        free(chan->drain_buf);
        chan->drain_buf      = NULL;
        chan->drain_buf_size = 0;
    }
    return COBALT_SUCCESS;
}
