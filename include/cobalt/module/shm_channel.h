/**
 * @file shm_channel.h
 * @brief Layer 2 Shared memory channel facade, wait policies and EventLoop integration
 */

#ifndef COBALT_MODULE_SHM_CHANNEL_H
#define COBALT_MODULE_SHM_CHANNEL_H

#include "cobalt/container/shm_ring.h"
#include "cobalt/module/eventloop.h"
#include "cobalt/runtime/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    COBALT_SHM_WAIT_SPIN     = 0, /* 100% spin, zero syscalls (<30ns) */
    COBALT_SHM_WAIT_BLOCKING = 1, /* Direct OS descriptor sleep */
    COBALT_SHM_WAIT_ADAPTIVE = 2  /* Spin threshold iterations then OS sleep */
} cobalt_shm_wait_policy_t;

typedef struct {
    const char              *name;              /* Shared memory name, e.g. "/cobalt_ipc_bus" */
    cobalt_shm_mode_t        mode;              /* COBALT_SHM_MODE_SPSC or COBALT_SHM_MODE_MPMC */
    cobalt_shm_wait_policy_t wait_policy;       /* SPIN / BLOCKING / ADAPTIVE */
    size_t                   capacity;          /* Power of 2 (bytes for SPSC, slots for MPMC) */
    size_t                   slot_size;         /* Max bytes per slot for MPMC; 0 for SPSC */
    bool                     create_if_missing; /* true: creator/server, false: attacher/client */
    uint32_t spin_threshold; /* Iterations before OS sleep in ADAPTIVE (default 1000) */
} cobalt_shm_config_t;

typedef struct cobalt_shm_channel cobalt_shm_channel_t;

/* Channel lifecycle */
cobalt_shm_channel_t *cobalt_shm_channel_create(const cobalt_shm_config_t *config);
cobalt_shm_channel_t *cobalt_shm_channel_open(const char *name, cobalt_shm_wait_policy_t policy);
void                  cobalt_shm_channel_destroy(cobalt_shm_channel_t *chan);

/* SPSC zero-copy reserve/commit/peek/consume */
int cobalt_shm_channel_reserve(cobalt_shm_channel_t *chan, size_t len, void **out_ptr);
int cobalt_shm_channel_commit(cobalt_shm_channel_t *chan, uint16_t msg_type, size_t len);
int cobalt_shm_channel_cancel(cobalt_shm_channel_t *chan);
int cobalt_shm_channel_peek(cobalt_shm_channel_t *chan,
                            uint16_t             *out_type,
                            const void          **out_ptr,
                            size_t               *out_len);
int cobalt_shm_channel_consume(cobalt_shm_channel_t *chan);

/* Convenience send/recv (works on both SPSC and MPMC) */
int cobalt_shm_channel_send(
    cobalt_shm_channel_t *chan, uint16_t msg_type, const void *data, size_t len, int timeout_ms);
int cobalt_shm_channel_recv(cobalt_shm_channel_t *chan,
                            uint16_t             *out_type,
                            void                 *buf,
                            size_t                buf_size,
                            size_t               *out_len,
                            int                   timeout_ms);

/* Accessors */
cobalt_shm_mode_t        cobalt_shm_channel_get_mode(const cobalt_shm_channel_t *chan);
cobalt_shm_wait_policy_t cobalt_shm_channel_get_wait_policy(const cobalt_shm_channel_t *chan);
int                      cobalt_shm_channel_get_read_fd(const cobalt_shm_channel_t *chan);

/* EventLoop Integration */
/**
 * @brief Message receive callback invoked when messages arrive from EventLoop.
 * @note cobalt_shm_channel_destroy must NOT be called directly within this callback.
 *       Call cobalt_shm_channel_detach_eventloop or defer destroy to the next loop tick.
 */
typedef void (*cobalt_shm_msg_cb)(cobalt_shm_channel_t *chan,
                                  uint16_t              msg_type,
                                  const void           *payload,
                                  size_t                len,
                                  void                 *user_data);

int cobalt_shm_channel_attach_eventloop(cobalt_shm_channel_t *chan,
                                        cobalt_eventloop_t   *loop,
                                        cobalt_shm_msg_cb     cb,
                                        void                 *user_data);

int cobalt_shm_channel_detach_eventloop(cobalt_shm_channel_t *chan);

#endif /* COBALT_MODULE_SHM_CHANNEL_H */
