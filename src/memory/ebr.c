#include "cobalt/memory/ebr.h"
#include "cobalt/platform/thread.h"
#include <stdalign.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef _WIN32
#include <pthread.h>
#include <time.h>
#else
#include <windows.h>
#endif

#define COBALT_CACHE_LINE_SIZE 64
#define COBALT_EBR_BATCH_LIMIT 64

/**
 * @brief Retired node entry representing deferred free memory
 */
typedef struct cobalt_ebr_node {
    void *ptr;
    void (*free_fn)(void *ptr, void *user_data);
    void                   *user_data;
    struct cobalt_ebr_node *next;
} cobalt_ebr_node_t;

/**
 * @brief Per-thread participant handle in the EBR engine
 */
struct cobalt_ebr_handle {
    atomic_uint_fast64_t                active_epoch;
    cobalt_ebr_node_t                  *local_retired[3];
    size_t                              retired_count;
    _Atomic(struct cobalt_ebr_handle *) next;
    struct cobalt_ebr                  *owner;
    atomic_bool                         registered;
};

/**
 * @brief Main EBR engine control structure
 */
struct cobalt_ebr {
    atomic_uint_fast64_t           global_epoch;
    _Atomic(cobalt_ebr_handle_t *) head_handle;
    cobalt_allocator_t            *allocator;
    atomic_size_t                  total_handles;

    /* Orphaned retirement pool from terminated or unregistered threads */
    cobalt_mutex_t    *orphan_lock;
    cobalt_ebr_node_t *orphaned_retired[3];
    uint64_t           id;
};

static void *ebr_alloc(cobalt_ebr_t *ebr, size_t size)
{
    if (ebr && ebr->allocator && ebr->allocator->alloc) {
        return ebr->allocator->alloc(ebr->allocator, size);
    }
    return malloc(size);
}

static void ebr_free(cobalt_ebr_t *ebr, void *ptr)
{
    if (!ptr) {
        return;
    }
    if (ebr && ebr->allocator && ebr->allocator->free) {
        ebr->allocator->free(ebr->allocator, ptr);
    } else {
        free(ptr);
    }
}

static void ebr_reclaim_node_list(cobalt_ebr_t *ebr, cobalt_ebr_node_t **head_ref)
{
    if (!head_ref || !*head_ref) {
        return;
    }
    cobalt_ebr_node_t *curr = *head_ref;
    *head_ref               = NULL;
    while (curr) {
        cobalt_ebr_node_t *next = curr->next;
        if (curr->ptr) {
            if (curr->free_fn) {
                curr->free_fn(curr->ptr, curr->user_data);
            } else {
                ebr_free(ebr, curr->ptr);
            }
        }
        ebr_free(ebr, curr);
        curr = next;
    }
}

static void ebr_reclaim_orphaned_slot(cobalt_ebr_t *ebr, size_t slot)
{
    if (!ebr || slot >= 3 || !ebr->orphan_lock) {
        return;
    }
    cobalt_ebr_node_t *to_free = NULL;
    cobalt_mutex_lock(ebr->orphan_lock);
    to_free                     = ebr->orphaned_retired[slot];
    ebr->orphaned_retired[slot] = NULL;
    cobalt_mutex_unlock(ebr->orphan_lock);

    if (to_free) {
        ebr_reclaim_node_list(ebr, &to_free);
    }
}

#ifndef _WIN32
static pthread_key_t  s_ebr_tls_key;
static pthread_once_t s_ebr_key_once = PTHREAD_ONCE_INIT;

static void ebr_tls_destructor(void *val)
{
    if (val) {
        cobalt_ebr_unregister((cobalt_ebr_handle_t *)val);
    }
}

static void ebr_init_tls_key(void)
{
    (void)pthread_key_create(&s_ebr_tls_key, ebr_tls_destructor);
}
#endif

static atomic_uint_fast64_t s_ebr_id_gen = 1;

cobalt_ebr_t *cobalt_ebr_create(cobalt_allocator_t *alloc)
{
    cobalt_allocator_t *effective_alloc = alloc ? alloc : cobalt_allocator_get_system();
    cobalt_ebr_t       *ebr             = NULL;

    if (effective_alloc && effective_alloc->alloc) {
        ebr = (cobalt_ebr_t *)effective_alloc->alloc(effective_alloc, sizeof(cobalt_ebr_t));
    } else {
        ebr = (cobalt_ebr_t *)malloc(sizeof(cobalt_ebr_t));
    }

    if (!ebr) {
        return NULL;
    }

    ebr->id = atomic_fetch_add_explicit(&s_ebr_id_gen, 1, memory_order_relaxed);
    atomic_init(&ebr->global_epoch, 0);
    atomic_init(&ebr->head_handle, NULL);
    atomic_init(&ebr->total_handles, 0);
    ebr->allocator = effective_alloc;

    ebr->orphan_lock = cobalt_mutex_create();
    for (size_t i = 0; i < 3; i++) {
        ebr->orphaned_retired[i] = NULL;
    }

#ifndef _WIN32
    (void)pthread_once(&s_ebr_key_once, ebr_init_tls_key);
#endif

    return ebr;
}

void cobalt_ebr_destroy(cobalt_ebr_t *ebr)
{
    if (!ebr) {
        return;
    }

    /* 1. Walk all registered handles and reclaim all their local retired lists */
    cobalt_ebr_handle_t *curr = atomic_load_explicit(&ebr->head_handle, memory_order_relaxed);
    while (curr) {
        cobalt_ebr_handle_t *next = atomic_load_explicit(&curr->next, memory_order_relaxed);
        atomic_store_explicit(&curr->registered, false, memory_order_relaxed);
        atomic_store_explicit(&curr->active_epoch, 0, memory_order_relaxed);
        for (size_t i = 0; i < 3; i++) {
            ebr_reclaim_node_list(ebr, &curr->local_retired[i]);
        }
        ebr_free(ebr, curr);
        curr = next;
    }

    /* 2. Reclaim all orphaned retired lists */
    for (size_t i = 0; i < 3; i++) {
        ebr_reclaim_node_list(ebr, &ebr->orphaned_retired[i]);
    }

    if (ebr->orphan_lock) {
        cobalt_mutex_destroy(ebr->orphan_lock);
        ebr->orphan_lock = NULL;
    }

    ebr_free(ebr, ebr);
}

cobalt_ebr_handle_t *cobalt_ebr_register(cobalt_ebr_t *ebr)
{
    if (!ebr) {
        return NULL;
    }

    /* Try to reuse an unregistered handle from the list first */
    cobalt_ebr_handle_t *curr = atomic_load_explicit(&ebr->head_handle, memory_order_acquire);
    while (curr) {
        bool expected = false;
        if (atomic_compare_exchange_strong_explicit(
                &curr->registered, &expected, true, memory_order_acq_rel, memory_order_acquire)) {
            atomic_store_explicit(&curr->active_epoch, 0, memory_order_release);
            curr->retired_count    = 0;
            curr->local_retired[0] = NULL;
            curr->local_retired[1] = NULL;
            curr->local_retired[2] = NULL;
#ifndef _WIN32
            pthread_setspecific(s_ebr_tls_key, curr);
#endif
            return curr;
        }
        curr = atomic_load_explicit(&curr->next, memory_order_acquire);
    }

    /* Allocate new handle aligned */
    cobalt_ebr_handle_t *handle =
        (cobalt_ebr_handle_t *)ebr_alloc(ebr, sizeof(cobalt_ebr_handle_t));
    if (!handle) {
        return NULL;
    }

    atomic_init(&handle->active_epoch, 0);
    handle->local_retired[0] = NULL;
    handle->local_retired[1] = NULL;
    handle->local_retired[2] = NULL;
    handle->retired_count    = 0;
    handle->owner            = ebr;
    atomic_init(&handle->registered, true);

    /* Push into lock-free list of handles */
    cobalt_ebr_handle_t *old_head = atomic_load_explicit(&ebr->head_handle, memory_order_relaxed);
    do {
        atomic_store_explicit(&handle->next, old_head, memory_order_relaxed);
    } while (!atomic_compare_exchange_weak_explicit(
        &ebr->head_handle, &old_head, handle, memory_order_release, memory_order_relaxed));

    atomic_fetch_add_explicit(&ebr->total_handles, 1, memory_order_relaxed);

#ifndef _WIN32
    pthread_setspecific(s_ebr_tls_key, handle);
#endif

    return handle;
}

void cobalt_ebr_unregister(cobalt_ebr_handle_t *handle)
{
    if (!handle) {
        return;
    }

    cobalt_ebr_t *ebr = handle->owner;

    /* Mark inactive */
    atomic_store_explicit(&handle->active_epoch, 0, memory_order_release);
    atomic_store_explicit(&handle->registered, false, memory_order_release);

    /* Transfer any remaining local retired nodes to orphaned list */
    if (ebr && ebr->orphan_lock) {
        cobalt_mutex_lock(ebr->orphan_lock);
        for (size_t i = 0; i < 3; i++) {
            if (handle->local_retired[i]) {
                cobalt_ebr_node_t *tail = handle->local_retired[i];
                while (tail->next) {
                    tail = tail->next;
                }
                tail->next               = ebr->orphaned_retired[i];
                ebr->orphaned_retired[i] = handle->local_retired[i];
                handle->local_retired[i] = NULL;
            }
        }
        cobalt_mutex_unlock(ebr->orphan_lock);
    }

    handle->retired_count = 0;
}

void cobalt_ebr_enter(cobalt_ebr_handle_t *handle)
{
    if (!handle) {
        return;
    }

    cobalt_ebr_t *ebr = handle->owner;
    uint64_t      g   = atomic_load_explicit(&ebr->global_epoch, memory_order_acquire);
    atomic_store_explicit(&handle->active_epoch, (g << 1) | 1, memory_order_seq_cst);

    /* Safe local maintenance: reclaim slot (g + 1) % 3 */
    size_t safe_slot = (g + 1) % 3;
    ebr_reclaim_node_list(ebr, &handle->local_retired[safe_slot]);
}

void cobalt_ebr_exit(cobalt_ebr_handle_t *handle)
{
    if (!handle) {
        return;
    }

    atomic_store_explicit(&handle->active_epoch, 0, memory_order_release);

    /* Safe local maintenance: reclaim safe slot on exit as well */
    cobalt_ebr_t *ebr       = handle->owner;
    uint64_t      g         = atomic_load_explicit(&ebr->global_epoch, memory_order_acquire);
    size_t        safe_slot = (g + 1) % 3;
    ebr_reclaim_node_list(ebr, &handle->local_retired[safe_slot]);
}

void cobalt_ebr_retire(cobalt_ebr_handle_t *handle,
                       void                *ptr,
                       void (*free_fn)(void *ptr, void *user_data),
                       void *user_data)
{
    if (!handle || !ptr) {
        return;
    }

    cobalt_ebr_t      *ebr  = handle->owner;
    cobalt_ebr_node_t *node = (cobalt_ebr_node_t *)ebr_alloc(ebr, sizeof(cobalt_ebr_node_t));
    if (!node) {
        if (free_fn) {
            free_fn(ptr, user_data);
        } else {
            ebr_free(ebr, ptr);
        }
        return;
    }

    node->ptr       = ptr;
    node->free_fn   = free_fn;
    node->user_data = user_data;

    uint64_t g                  = atomic_load_explicit(&ebr->global_epoch, memory_order_relaxed);
    size_t   slot               = g % 3;
    node->next                  = handle->local_retired[slot];
    handle->local_retired[slot] = node;
    handle->retired_count++;

    /* Safe maintenance: reclaim slot (g + 1) % 3 */
    size_t safe_slot = (g + 1) % 3;
    ebr_reclaim_node_list(ebr, &handle->local_retired[safe_slot]);

    /* If batch threshold reached, attempt to advance global epoch */
    if (handle->retired_count >= COBALT_EBR_BATCH_LIMIT) {
        handle->retired_count = 0;
        cobalt_ebr_try_advance(ebr);
    }
}

bool cobalt_ebr_try_advance(cobalt_ebr_t *ebr)
{
    if (!ebr) {
        return false;
    }

    uint64_t g = atomic_load_explicit(&ebr->global_epoch, memory_order_acquire);

    /* Check all registered threads */
    cobalt_ebr_handle_t *curr = atomic_load_explicit(&ebr->head_handle, memory_order_acquire);
    while (curr) {
        if (atomic_load_explicit(&curr->registered, memory_order_acquire)) {
            uint64_t a = atomic_load_explicit(&curr->active_epoch, memory_order_acquire);
            if ((a & 1) != 0) {
                uint64_t thread_epoch = a >> 1;
                if (thread_epoch < g) {
                    /* A thread is lagging in an earlier epoch, cannot advance */
                    return false;
                }
            }
        }
        curr = atomic_load_explicit(&curr->next, memory_order_acquire);
    }

    /* All active threads are at >= g; attempt CAS advance */
    if (!atomic_compare_exchange_strong_explicit(
            &ebr->global_epoch, &g, g + 1, memory_order_acq_rel, memory_order_acquire)) {
        return false;
    }

    /* With new epoch (g + 1), slot (g + 1 + 1) % 3 = (g + 2) % 3 is safe to reclaim */
    size_t safe_slot = (g + 2) % 3;
    ebr_reclaim_orphaned_slot(ebr, safe_slot);

    return true;
}

void cobalt_ebr_sync(cobalt_ebr_t *ebr)
{
    if (!ebr) {
        return;
    }

    uint64_t start  = atomic_load_explicit(&ebr->global_epoch, memory_order_acquire);
    uint64_t target = start + 3;

    while (atomic_load_explicit(&ebr->global_epoch, memory_order_acquire) < target) {
        if (!cobalt_ebr_try_advance(ebr)) {
#ifndef _WIN32
            struct timespec ts = {0, 1000000}; /* 1 ms */
            nanosleep(&ts, NULL);
#else
            Sleep(1);
#endif
        }
    }

    /* Flush orphaned lists */
    for (size_t i = 0; i < 3; i++) {
        ebr_reclaim_orphaned_slot(ebr, i);
    }
}

uint64_t cobalt_ebr_get_id(const cobalt_ebr_t *ebr)
{
    return ebr ? ebr->id : 0;
}
