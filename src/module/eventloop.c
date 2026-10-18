/**
 * @file eventloop.c
 * @brief Implementation of the event-driven I/O loop module
 */
#define _POSIX_C_SOURCE 200112L
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "cobalt/module/eventloop.h"
#include "cobalt/container/lfqueue.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/platform/thread.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef _WIN32
#include <poll.h>
#endif

#ifdef __linux__
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#elif __APPLE__
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/un.h>
#elif defined(_WIN32)
/* MinGW does not provide poll.h; Windows backend is not supported. */
#ifndef POLLIN
#define POLLIN 0x0001
#endif
#ifndef POLLOUT
#define POLLOUT 0x0004
#endif
#ifndef POLLERR
#define POLLERR 0x0008
#endif
#ifndef POLLNVAL
#define POLLNVAL 0x0020
#endif
#else
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

#define COBALT_EVENTLOOP_DEFAULT_CAPACITY 16
#define COBALT_EVENTLOOP_TIMEOUT_MS 10
#define COBALT_SIGNAL_RING_SIZE 64
#define COBALT_MILLIS_PER_SEC 1000ULL
#define COBALT_NANOS_PER_MILLI 1000000ULL
#define COBALT_NANOS_PER_SEC 1000000000LL

#define COBALT_TW_L0_BITS 8
#define COBALT_TW_L0_SIZE (1 << COBALT_TW_L0_BITS) /* 256 */
#define COBALT_TW_L0_MASK (COBALT_TW_L0_SIZE - 1)

#define COBALT_TW_LN_BITS 6
#define COBALT_TW_LN_SIZE (1 << COBALT_TW_LN_BITS) /* 64 */
#define COBALT_TW_LN_MASK (COBALT_TW_LN_SIZE - 1)

#define COBALT_TW_L1_SHIFT 8
#define COBALT_TW_L2_SHIFT 14
#define COBALT_TW_L3_SHIFT 20

#define COBALT_EVENTLOOP_DEFAULT_POST_CAPACITY 65536

/* -------------------------------------------------------------------------- */
/* Internal types                                                             */
/* -------------------------------------------------------------------------- */
/**
 * @brief Posted task wrapper stored in post_queue
 */
typedef struct {
    cobalt_loop_task_fn_t fn;
    void                 *arg;
} cobalt_post_task_t;

/**
 * @brief Intrusive timer node stored in the hierarchical timing wheel
 */
typedef struct cobalt_timer_node {
    uint64_t                  timer_id;       /**< Unique id returned to the caller */
    uint64_t                  expire_time_ms; /**< Monotonic target fire time in ms */
    uint64_t                  interval_ms;    /**< Repeat interval (0 for one-shot) */
    timer_handler_t           callback;       /**< Function invoked on expiry */
    void                     *user_data;      /**< Opaque callback argument */
    struct cobalt_timer_node *prev;           /**< Doubly-linked slot list prev */
    struct cobalt_timer_node *next;           /**< Doubly-linked slot list next */
    uint8_t                   level;          /**< Current timing wheel level (0..3) */
    uint8_t                   slot;           /**< Current slot index within level */
    bool                      active;         /**< True while the timer is armed */
    bool                      firing;         /**< True while callback is executing */
    bool                      deleted;        /**< True if cancelled during callback */
} cobalt_timer_node_t;

/**
 * @brief Doubly-linked list head/tail for each slot
 */
typedef struct {
    cobalt_timer_node_t *head;
    cobalt_timer_node_t *tail;
} timer_slot_t;

/**
 * @brief 4-level Hierarchical Timing Wheel structure
 */
typedef struct {
    timer_slot_t slots_l0[COBALT_TW_L0_SIZE];
    timer_slot_t slots_ln[3][COBALT_TW_LN_SIZE];
    uint64_t     wheel_time_ms;
} cobalt_timing_wheel_t;

/**
 * @brief Hash table entry mapping timer_id -> cobalt_timer_node_t* for O(1) cancel
 */
typedef struct {
    uint64_t             timer_id;
    cobalt_timer_node_t *node;
} timer_table_entry_t;

/* Largest power-of-two timer table capacity. 2^30 * 16 bytes = 16 GiB; the
 * `new_cap - 1` probe mask stays a power of two. */
#define COBALT_TIMER_TABLE_MAX_CAP (1u << 30)

/**
 * @brief Open-addressing hash table for O(1) timer cancellation
 */
typedef struct {
    timer_table_entry_t *entries;
    size_t               capacity;
    size_t               count;
} timer_table_t;

/**
 * @brief File descriptor registration in the fd list
 */
typedef struct fd_entry {
    int              fd;        /**< Watched file descriptor */
    short            events;    /**< Subscribed event mask */
    fd_handler_t     callback;  /**< Function invoked on readiness */
    void            *user_data; /**< Opaque callback argument */
    struct fd_entry *next;      /**< Next registration in the list */
} fd_entry_t;

/* Signal ring buffer — async-signal-safe, fixed size */
static int                   g_signal_ring[COBALT_SIGNAL_RING_SIZE];
static volatile sig_atomic_t g_signal_head  = 0;
static volatile sig_atomic_t g_signal_tail  = 0;
static volatile sig_atomic_t g_signal_count = 0;

/* Signal handler table (signum -> callback + user_data) */
/**
 * @brief Signal handler table entry mapping one signal to its callback
 */
typedef struct {
    fd_handler_t callback;  /**< Function invoked on deferred dispatch */
    void        *user_data; /**< Opaque callback argument */
    int          signum;    /**< Signal number handled */
    int          active;    /**< Non-zero while the slot is registered */
} signal_handler_entry_t;

#define COBALT_MAX_SIGNAL_HANDLERS 32
static signal_handler_entry_t g_signal_table[COBALT_MAX_SIGNAL_HANDLERS];

/**
 * @brief Opaque event loop structure
 * @details Owns the platform wait handle (epoll/kqueue/poll), the fd list,
 *          the hierarchical timing wheel, and the signal dispatch table.
 */
struct cobalt_eventloop {
#ifdef __linux__
    int                 epoll_fd;
    struct epoll_event *epoll_events;
    int                 epoll_capacity;
#elif __APPLE__
    int kq;
#elif defined(_WIN32)
    /* No backend fields for unsupported Windows port */
#else
    struct pollfd *pollfds;
    int            pollfds_capacity;
#endif
    fd_entry_t           *fd_head;
    fd_entry_t           *fd_tail;
    cobalt_timing_wheel_t wheel;
    timer_table_t         timer_table;
    int                   timer_count;
    uint64_t              next_timer_id;
    int                   running;
    int                   stop_flag;
    /* Cross-thread task dispatch and async wakeup */
    cobalt_lfqueue_t *post_queue;
    int               wakeup_fd;
#ifndef __linux__
    int wakeup_write_fd;
#endif
    fd_entry_t   wakeup_entry;
    _Atomic bool is_sleeping;
    /* Close callback */
    fd_handler_t        close_callback;
    void               *close_user_data;
    cobalt_allocator_t *alloc;
};

/* -------------------------------------------------------------------------- */
/* Signal handling                                                            */
/* -------------------------------------------------------------------------- */
/**
 * @brief Record a caught signal into the ring buffer for deferred dispatch.
 *
 * @param signum Signal number caught by the OS handler.
 *
 * @note Async-signal-safe: only touches sig_atomic_t state, never calls back directly.
 */
#ifndef _WIN32
static void cobalt_signal_handler(int signum)
{
    /* Ring-buffer write is async-signal-safe (no malloc, no stdio) */
    int next = (g_signal_tail + 1) % COBALT_SIGNAL_RING_SIZE;
    if (next != (int)g_signal_head) {
        g_signal_ring[(int)g_signal_tail] = signum;
        g_signal_tail                     = (sig_atomic_t)next;
        g_signal_count++;
    }
}
#endif

/**
 * @brief Dispatch all signals pending in the ring buffer to registered handlers.
 *
 * @param loop Unused; signal state is process-global.
 */
static void cobalt_drain_signals(cobalt_eventloop_t *loop)
{
    (void)loop;
    while (g_signal_count > 0) {
        int signum    = g_signal_ring[(int)g_signal_head];
        g_signal_head = (sig_atomic_t)((g_signal_head + 1) % COBALT_SIGNAL_RING_SIZE);
        g_signal_count--;

        for (int i = 0; i < COBALT_MAX_SIGNAL_HANDLERS; i++) {
            if (g_signal_table[i].active && g_signal_table[i].signum == signum) {
                g_signal_table[i].callback((cobalt_fd_t)signum, 0, g_signal_table[i].user_data);
                break;
            }
        }
    }
}

/*/* -------------------------------------------------------------------------- */
/* Timing Wheel and Timer Table Operations                                    */
/* -------------------------------------------------------------------------- */
/**
 * @brief Sample the monotonic clock.
 *
 * @param ts Receives the current monotonic time.
 */
static void get_time_now(struct timespec *ts)
{
    clock_gettime(CLOCK_MONOTONIC, ts);
}

/**
 * @brief Return a pointer to the slot at the specified wheel level and index.
 */
static inline timer_slot_t *
wheel_get_slot(cobalt_timing_wheel_t *wheel, uint8_t level, uint8_t slot)
{
    if (level == 0) {
        return &wheel->slots_l0[slot];
    }
    if (level <= 3) {
        return &wheel->slots_ln[level - 1][slot];
    }
    return NULL;
}

/**
 * @brief Append an intrusive timer node to the tail of a slot list.
 */
static void slot_append(timer_slot_t *slot, cobalt_timer_node_t *node)
{
    node->next = NULL;
    node->prev = slot->tail;
    if (slot->tail) {
        slot->tail->next = node;
    } else {
        slot->head = node;
    }
    slot->tail = node;
}

/**
 * @brief Unlink an intrusive timer node from a slot list in O(1).
 */
static void slot_remove(timer_slot_t *slot, cobalt_timer_node_t *node)
{
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        slot->head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        slot->tail = node->prev;
    }
    node->prev = NULL;
    node->next = NULL;
}

/**
 * @brief Thomas Wang's 64-bit integer mix hash function.
 */
static inline uint64_t timer_hash64(uint64_t key)
{
    key = (~key) + (key << 21);
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8);
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return key;
}

/**
 * @brief Insert or update a timer node in the O(1) hash table.
 */
static int timer_table_insert(cobalt_eventloop_t  *loop,
                              timer_table_t       *table,
                              uint64_t             timer_id,
                              cobalt_timer_node_t *node)
{
    if (table->capacity == 0 || table->count * 10 >= table->capacity * 7) {
        size_t new_cap;
        if (table->capacity == 0) {
            new_cap = 64;
        } else if (table->capacity >= COBALT_TIMER_TABLE_MAX_CAP) {
            new_cap = table->capacity; /* capped: retry same size; OOM -> -1 */
        } else {
            new_cap = table->capacity * 2 > COBALT_TIMER_TABLE_MAX_CAP ? COBALT_TIMER_TABLE_MAX_CAP
                                                                       : table->capacity * 2;
        }
        timer_table_entry_t *new_entries = (timer_table_entry_t *)loop->alloc->alloc(
            loop->alloc, sizeof(timer_table_entry_t) * new_cap);
        if (!new_entries) {
            return -1;
        }
        memset(new_entries, 0, sizeof(timer_table_entry_t) * new_cap);

        for (size_t i = 0; i < table->capacity; i++) {
            if (table->entries[i].node != NULL) {
                size_t idx = (size_t)(timer_hash64(table->entries[i].timer_id) & (new_cap - 1));
                while (new_entries[idx].node != NULL) {
                    idx = (idx + 1) & (new_cap - 1);
                }
                new_entries[idx] = table->entries[i];
            }
        }
        if (table->entries) {
            loop->alloc->free(loop->alloc, table->entries);
        }
        table->entries  = new_entries;
        table->capacity = new_cap;
    }

    size_t mask = table->capacity - 1;
    size_t idx  = (size_t)(timer_hash64(timer_id) & mask);
    while (table->entries[idx].node != NULL) {
        if (table->entries[idx].timer_id == timer_id) {
            table->entries[idx].node = node;
            return 0;
        }
        idx = (idx + 1) & mask;
    }
    table->entries[idx].timer_id = timer_id;
    table->entries[idx].node     = node;
    table->count++;
    return 0;
}

/**
 * @brief Find a timer node by its id in O(1).
 */
static cobalt_timer_node_t *timer_table_find(const timer_table_t *table, uint64_t timer_id)
{
    if (!table->entries || table->count == 0) {
        return NULL;
    }
    size_t mask = table->capacity - 1;
    size_t idx  = (size_t)(timer_hash64(timer_id) & mask);
    while (table->entries[idx].node != NULL) {
        if (table->entries[idx].timer_id == timer_id) {
            return table->entries[idx].node;
        }
        idx = (idx + 1) & mask;
    }
    return NULL;
}

/**
 * @brief Remove a timer entry from the hash table using backward-shift deletion.
 */
static void timer_table_remove(timer_table_t *table, uint64_t timer_id)
{
    if (!table->entries || table->count == 0) {
        return;
    }
    size_t mask = table->capacity - 1;
    size_t idx  = (size_t)(timer_hash64(timer_id) & mask);
    while (table->entries[idx].node != NULL) {
        if (table->entries[idx].timer_id == timer_id) {
            size_t curr = idx;
            size_t next = (curr + 1) & mask;
            while (table->entries[next].node != NULL) {
                size_t natural   = (size_t)(timer_hash64(table->entries[next].timer_id) & mask);
                int    can_shift = 0;
                if (next > curr) {
                    if (natural <= curr || natural > next) {
                        can_shift = 1;
                    }
                } else {
                    if (natural <= curr && natural > next) {
                        can_shift = 1;
                    }
                }
                if (can_shift) {
                    table->entries[curr] = table->entries[next];
                    curr                 = next;
                }
                next = (next + 1) & mask;
            }
            table->entries[curr].node     = NULL;
            table->entries[curr].timer_id = 0;
            table->count--;
            return;
        }
        idx = (idx + 1) & mask;
    }
}

/**
 * @brief Release hash table storage.
 */
static void timer_table_destroy(cobalt_allocator_t *alloc, timer_table_t *table)
{
    if (table->entries) {
        alloc->free(alloc, table->entries);
        table->entries  = NULL;
        table->capacity = 0;
        table->count    = 0;
    }
}

/**
 * @brief Insert a timer node into the appropriate wheel level and slot.
 */
static void wheel_insert_node(cobalt_eventloop_t *loop, cobalt_timer_node_t *node)
{
    uint64_t cur = loop->wheel.wheel_time_ms;
    uint64_t exp = node->expire_time_ms;

    if (exp <= cur) {
        uint8_t slot = (uint8_t)(cur & COBALT_TW_L0_MASK);
        node->level  = 0;
        node->slot   = slot;
        slot_append(&loop->wheel.slots_l0[slot], node);
        return;
    }

    uint64_t delta = exp - cur;

    if (delta < (1ULL << COBALT_TW_L0_BITS)) {
        /* Level 0: 0..255 ms */
        uint8_t slot = (uint8_t)(exp & COBALT_TW_L0_MASK);
        node->level  = 0;
        node->slot   = slot;
        slot_append(&loop->wheel.slots_l0[slot], node);
    } else if (delta < (1ULL << COBALT_TW_L2_SHIFT)) {
        /* Level 1: 256 ms .. 16383 ms (shift 8) */
        uint8_t slot = (uint8_t)((exp >> COBALT_TW_L1_SHIFT) & COBALT_TW_LN_MASK);
        node->level  = 1;
        node->slot   = slot;
        slot_append(&loop->wheel.slots_ln[0][slot], node);
    } else if (delta < (1ULL << COBALT_TW_L3_SHIFT)) {
        /* Level 2: ~16.38 s .. ~17.47 min (shift 14) */
        uint8_t slot = (uint8_t)((exp >> COBALT_TW_L2_SHIFT) & COBALT_TW_LN_MASK);
        node->level  = 2;
        node->slot   = slot;
        slot_append(&loop->wheel.slots_ln[1][slot], node);
    } else {
        /* Level 3: ~17.47 min .. ~18.64 hours (shift 20) */
        uint8_t slot = (uint8_t)((exp >> COBALT_TW_L3_SHIFT) & COBALT_TW_LN_MASK);
        node->level  = 3;
        node->slot   = slot;
        slot_append(&loop->wheel.slots_ln[2][slot], node);
    }
}

/**
 * @brief Cascade all timers in a higher-level slot down into lower levels.
 */
static void cascade_level(cobalt_eventloop_t *loop, uint8_t level, uint8_t slot)
{
    if (level < 1 || level > 3) {
        return;
    }
    timer_slot_t        *s    = &loop->wheel.slots_ln[level - 1][slot];
    cobalt_timer_node_t *node = s->head;
    s->head                   = NULL;
    s->tail                   = NULL;

    while (node) {
        cobalt_timer_node_t *next = node->next;
        node->prev                = NULL;
        node->next                = NULL;
        wheel_insert_node(loop, node);
        node = next;
    }
}

/**
 * @brief Fire all due timers in a Level 0 slot.
 */
static void process_slot_timers(cobalt_eventloop_t *loop, uint8_t slot_idx)
{
    timer_slot_t        *s    = &loop->wheel.slots_l0[slot_idx];
    cobalt_timer_node_t *node = s->head;
    s->head                   = NULL;
    s->tail                   = NULL;

    while (node) {
        cobalt_timer_node_t *next = node->next;
        node->prev                = NULL;
        node->next                = NULL;

        if (node->active && !node->deleted) {
            uint64_t        timer_id  = node->timer_id;
            timer_handler_t cb        = node->callback;
            void           *user_data = node->user_data;
            uint64_t        interval  = node->interval_ms;

            node->firing = true;
            if (cb) {
                cb(timer_id, user_data);
            }
            node->firing = false;

            if (interval > 0 && node->active && !node->deleted) {
                node->expire_time_ms += interval;
                if (node->expire_time_ms <= loop->wheel.wheel_time_ms) {
                    node->expire_time_ms = loop->wheel.wheel_time_ms + interval;
                }
                wheel_insert_node(loop, node);
            } else {
                if (!node->deleted) {
                    timer_table_remove(&loop->timer_table, timer_id);
                    loop->timer_count--;
                }
                loop->alloc->free(loop->alloc, node);
            }
        } else {
            if (!node->deleted) {
                timer_table_remove(&loop->timer_table, node->timer_id);
                loop->timer_count--;
            }
            loop->alloc->free(loop->alloc, node);
        }

        node = next;
    }
}

/**
 * @brief Advance the timing wheel tick-by-tick up to target_time_ms.
 */
static void wheel_advance(cobalt_eventloop_t *loop, uint64_t target_time_ms)
{
    while (loop->wheel.wheel_time_ms < target_time_ms) {
        loop->wheel.wheel_time_ms++;
        uint64_t cur    = loop->wheel.wheel_time_ms;
        uint8_t  l0_idx = (uint8_t)(cur & COBALT_TW_L0_MASK);

        /* Cascading from higher levels at wrap boundaries */
        if (l0_idx == 0) {
            uint8_t l1_idx = (uint8_t)((cur >> COBALT_TW_L1_SHIFT) & COBALT_TW_LN_MASK);
            cascade_level(loop, 1, l1_idx);

            if (l1_idx == 0) {
                uint8_t l2_idx = (uint8_t)((cur >> COBALT_TW_L2_SHIFT) & COBALT_TW_LN_MASK);
                cascade_level(loop, 2, l2_idx);

                if (l2_idx == 0) {
                    uint8_t l3_idx = (uint8_t)((cur >> COBALT_TW_L3_SHIFT) & COBALT_TW_LN_MASK);
                    cascade_level(loop, 3, l3_idx);
                }
            }
        }

        process_slot_timers(loop, l0_idx);
    }
}

/**
 * @brief Compute how long the backend may block before the next timer fires.
 *
 * @param loop Event loop owning the timing wheel.
 * @param now Current time, or NULL to sample the monotonic clock.
 * @return Milliseconds to wait; never negative.
 */
#ifndef _WIN32
static int calculate_timeout_ms(const cobalt_eventloop_t *loop, const struct timespec *now)
{
    if (loop->timer_count == 0) {
        return COBALT_EVENTLOOP_TIMEOUT_MS;
    }

    uint64_t now_ms;
    if (now) {
        now_ms = (uint64_t)now->tv_sec * 1000ULL + (uint64_t)now->tv_nsec / 1000000ULL;
    } else {
        struct timespec ts;
        get_time_now(&ts);
        now_ms = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }

    /* Scan Level 0 for the nearest non-empty slot */
    for (uint32_t offset = 1; offset <= COBALT_TW_L0_SIZE; offset++) {
        uint8_t slot = (uint8_t)((loop->wheel.wheel_time_ms + offset) & COBALT_TW_L0_MASK);
        if (loop->wheel.slots_l0[slot].head != NULL) {
            uint64_t target_ms = loop->wheel.wheel_time_ms + offset;
            if (target_ms <= now_ms) {
                return 0;
            }
            int diff = (int)(target_ms - now_ms);
            return diff < COBALT_EVENTLOOP_TIMEOUT_MS ? diff : COBALT_EVENTLOOP_TIMEOUT_MS;
        }
    }

    /* Level 0 is empty: wait at most default short timeout */
    return COBALT_EVENTLOOP_TIMEOUT_MS;
}
#endif

/* -------------------------------------------------------------------------- */
/* Cross-Thread Task Dispatch & Wakeup Helpers                                */
/* -------------------------------------------------------------------------- */
#ifndef _WIN32
/**
 * @brief Drain any bytes pending in the wakeup file descriptor.
 */
static void drain_wakeup_fd(cobalt_eventloop_t *loop)
{
#if defined(__linux__)
    if (loop->wakeup_fd >= 0) {
        uint64_t val;
        while (read(loop->wakeup_fd, &val, sizeof(val)) > 0) {
        }
    }
#else
    if (loop->wakeup_fd >= 0) {
        char buf[128];
        while (read(loop->wakeup_fd, buf, sizeof(buf)) > 0) {
        }
    }
#endif
}
#endif

/**
 * @brief Dequeue and execute all tasks posted to the loop's lock-free queue.
 */
static void drain_post_queue(cobalt_eventloop_t *loop)
{
    if (!loop->post_queue) {
        return;
    }
    void *item = NULL;
    while (cobalt_lfqueue_dequeue(loop->post_queue, &item) == 0 && item != NULL) {
        cobalt_post_task_t *task = (cobalt_post_task_t *)item;
        if (task->fn) {
            task->fn(task->arg);
        }
        loop->alloc->free(loop->alloc, task);
    }
}

#ifndef _WIN32
/**
 * @brief Wakeup handler invoked when wakeup_fd becomes readable.
 */
static void on_wakeup_event(cobalt_fd_t fd, cobalt_events_t events, void *user_data)
{
    (void)fd;
    (void)events;
    cobalt_eventloop_t *loop = (cobalt_eventloop_t *)user_data;
    drain_wakeup_fd(loop);
    drain_post_queue(loop);
}
#endif

/**
 * @brief Signal the kernel wakeup descriptor to unblock platform_wait.
 */
static void wakeup_loop(cobalt_eventloop_t *loop)
{
#if defined(__linux__)
    if (loop->wakeup_fd >= 0) {
        uint64_t one = 1;
        ssize_t  n   = write(loop->wakeup_fd, &one, sizeof(one));
        (void)n;
    }
#elif !defined(_WIN32)
    if (loop->wakeup_write_fd >= 0) {
        char    b = 1;
        ssize_t n = write(loop->wakeup_write_fd, &b, 1);
        (void)n;
    }
#else
    (void)loop;
#endif
}

/* -------------------------------------------------------------------------- */
/* Platform-specific FD management                                            */
/* -------------------------------------------------------------------------- */
/**
 * @brief Register a file descriptor with the platform wait backend.
 *
 * @param loop Event loop owning the backend state.
 * @param fd Descriptor to watch.
 * @param events Event mask to wait for.
 * @return 0 on success, nonzero when the backend rejects the descriptor.
 */
static int platform_add_fd(cobalt_eventloop_t *loop, int fd, short events)
{
#ifdef __linux__
    struct epoll_event ev = {0};
    ev.events             = events;
    ev.data.ptr           = NULL;
    return epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
#elif __APPLE__
    (void)events;
    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    return kevent(loop->kq, &kev, 1, NULL, 0, NULL);
#elif defined(_WIN32)
    (void)loop;
    (void)fd;
    (void)events;
    return -1;
#else
    int idx = fd;
    if (idx >= loop->pollfds_capacity) {
        int max_cap = (int)(INT_MAX / sizeof(struct pollfd));
        int new_cap;
        if (loop->pollfds_capacity == 0) {
            new_cap = COBALT_EVENTLOOP_DEFAULT_CAPACITY;
        } else if (loop->pollfds_capacity > max_cap / 2) {
            new_cap = max_cap; /* capped: retry same size; OOM -> -1 */
        } else {
            new_cap = loop->pollfds_capacity * 2;
        }
        struct pollfd *new_pfds = (struct pollfd *)loop->alloc->realloc(
            loop->alloc, loop->pollfds, sizeof(struct pollfd) * new_cap);
        if (!new_pfds) {
            return -1;
        }
        memset(new_pfds + loop->pollfds_capacity,
               0,
               sizeof(struct pollfd) * (new_cap - loop->pollfds_capacity));
        loop->pollfds          = new_pfds;
        loop->pollfds_capacity = new_cap;
    }
    loop->pollfds[idx].fd     = fd;
    loop->pollfds[idx].events = events;
    return 0;
#endif
}

/**
 * @brief Remove a descriptor from the platform wait backend.
 *
 * @param loop Event loop owning the backend state.
 * @param fd Descriptor to stop watching.
 * @return 0 on success, nonzero when the backend rejects the removal.
 */
static int platform_del_fd(cobalt_eventloop_t *loop, int fd)
{
#ifdef __linux__
    return epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
#elif __APPLE__
    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    return kevent(loop->kq, &kev, 1, NULL, 0, NULL);
#elif defined(_WIN32)
    (void)loop;
    (void)fd;
    return 0;
#else
    if (fd < 0 || fd >= loop->pollfds_capacity) {
        return 0;
    }
    loop->pollfds[fd].fd     = -1;
    loop->pollfds[fd].events = 0;
    return 0;
#endif
}

/**
 * @brief Block until backend events arrive or the timer timeout elapses.
 *
 * @param loop Event loop owning the backend state.
 * @param event_count_out Receives the number of ready events.
 * @param entries_out Receives the backend event array.
 * @param max_events Capacity of the caller event buffer (poll backend only).
 */
static void
platform_wait(cobalt_eventloop_t *loop, int *event_count_out, void **entries_out, int max_events)
{
    (void)max_events;
#ifdef __linux__
    int timeout_ms = calculate_timeout_ms(loop, NULL);
    *event_count_out =
        epoll_wait(loop->epoll_fd, loop->epoll_events, loop->epoll_capacity, timeout_ms);
    *entries_out = loop->epoll_events;
#elif __APPLE__
    int             timeout_ms = calculate_timeout_ms(loop, NULL);
    struct timespec ts         = {timeout_ms / 1000, (timeout_ms % 1000) * COBALT_NANOS_PER_MILLI};
    struct kevent   events[COBALT_EVENTLOOP_DEFAULT_CAPACITY];
    *event_count_out = kevent(loop->kq, NULL, 0, events, COBALT_EVENTLOOP_DEFAULT_CAPACITY, &ts);
    *entries_out     = events;
#elif defined(_WIN32)
    (void)loop;
    (void)max_events;
    *event_count_out = 0;
    *entries_out     = NULL;
#else
    int timeout_ms   = calculate_timeout_ms(loop, NULL);
    *event_count_out = poll(loop->pollfds, (nfds_t)loop->pollfds_capacity, timeout_ms);
    *entries_out     = loop->pollfds;
    (void)max_events;
#endif
}

/* -------------------------------------------------------------------------- */
/* UNIX domain socket helpers                                                 */
/* -------------------------------------------------------------------------- */

int cobalt_eventloop_create_unix_server(const char *path, cobalt_fd_t *sock_out)
{
#ifndef _WIN32
    if (!path || !sock_out) {
        return -1;
    }

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    struct sockaddr_un addr = {};
    addr.sun_family         = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    /* Remove stale socket file */
    unlink(path);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    if (listen(sock, 128) < 0) {
        close(sock);
        unlink(path);
        return -1;
    }

    *sock_out = (cobalt_fd_t)sock;
    return 0;
#else
    (void)path;
    (void)sock_out;
    return -1;
#endif
}

int cobalt_eventloop_accept(cobalt_eventloop_t *loop,
                            cobalt_fd_t         listen_fd,
                            cobalt_events_t     events,
                            fd_handler_t        callback,
                            void               *user_data)
{
#ifndef _WIN32
    if (!loop || listen_fd < 0 || !callback) {
        return -1;
    }

    struct sockaddr_un client_addr = {};
    socklen_t          addrlen     = sizeof(client_addr);
    cobalt_fd_t        conn_fd = accept((int)listen_fd, (struct sockaddr *)&client_addr, &addrlen);
    if (conn_fd < 0) {
        return -1;
    }

    int ret = cobalt_eventloop_add_fd(loop, conn_fd, events, callback, user_data);
    if (ret < 0) {
        close(conn_fd);
    }
    return ret;
#else
    (void)loop;
    (void)listen_fd;
    (void)events;
    (void)callback;
    (void)user_data;
    return -1;
#endif
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */
/**
 * @brief Allocate and initialize an event loop with an explicit allocator.
 *
 * @param alloc Allocator used for the loop and all its state.
 * @return New event loop, or NULL when allocation or backend setup fails.
 */
static cobalt_eventloop_t *cobalt_eventloop_create_with_alloc(cobalt_allocator_t *alloc)
{
    cobalt_eventloop_t *loop =
        (cobalt_eventloop_t *)alloc->alloc(alloc, sizeof(cobalt_eventloop_t));
    if (!loop) {
        return NULL;
    }
    memset(loop, 0, sizeof(cobalt_eventloop_t));
    loop->alloc = alloc;

#ifdef __linux__
    loop->epoll_fd = epoll_create1(0);
    if (loop->epoll_fd < 0) {
        loop->alloc->free(loop->alloc, loop);
        return NULL;
    }
    loop->epoll_capacity = COBALT_EVENTLOOP_DEFAULT_CAPACITY;
    loop->epoll_events   = (struct epoll_event *)alloc->alloc(
        alloc, sizeof(struct epoll_event) * loop->epoll_capacity);
    if (!loop->epoll_events) {
        close(loop->epoll_fd);
        loop->alloc->free(loop->alloc, loop);
        return NULL;
    }
#elif __APPLE__
    loop->kq = kqueue();
    if (loop->kq < 0) {
        loop->alloc->free(loop->alloc, loop);
        return NULL;
    }
#endif

    memset(&loop->wheel, 0, sizeof(loop->wheel));
    struct timespec now;
    get_time_now(&now);
    loop->wheel.wheel_time_ms = (uint64_t)now.tv_sec * 1000ULL + (uint64_t)now.tv_nsec / 1000000ULL;
    if (loop->wheel.wheel_time_ms > 0) {
        loop->wheel.wheel_time_ms--;
    }
    memset(&loop->timer_table, 0, sizeof(loop->timer_table));
    loop->timer_count = 0;

    /* Initialize post queue and wakeup descriptor */
    loop->post_queue =
        cobalt_lfqueue_create_with_allocator(COBALT_EVENTLOOP_DEFAULT_POST_CAPACITY, alloc);
    atomic_init(&loop->is_sleeping, false);
    loop->wakeup_fd = -1;
#ifndef __linux__
    loop->wakeup_write_fd = -1;
#endif

#if defined(__linux__)
    loop->wakeup_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (loop->wakeup_fd >= 0) {
        loop->wakeup_entry.fd        = loop->wakeup_fd;
        loop->wakeup_entry.events    = POLLIN;
        loop->wakeup_entry.callback  = on_wakeup_event;
        loop->wakeup_entry.user_data = loop;

        struct epoll_event ev = {0};
        ev.events             = EPOLLIN;
        ev.data.ptr           = &loop->wakeup_entry;
        epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, loop->wakeup_fd, &ev);
    }
#elif !defined(_WIN32)
    int pfd[2];
    if (pipe(pfd) == 0) {
        int flags0 = fcntl(pfd[0], F_GETFL, 0);
        fcntl(pfd[0], F_SETFL, (flags0 >= 0 ? flags0 : 0) | O_NONBLOCK);
        int flags1 = fcntl(pfd[1], F_GETFL, 0);
        fcntl(pfd[1], F_SETFL, (flags1 >= 0 ? flags1 : 0) | O_NONBLOCK);
        loop->wakeup_fd       = pfd[0];
        loop->wakeup_write_fd = pfd[1];

        loop->wakeup_entry.fd        = loop->wakeup_fd;
        loop->wakeup_entry.events    = POLLIN;
        loop->wakeup_entry.callback  = on_wakeup_event;
        loop->wakeup_entry.user_data = loop;

#if defined(__APPLE__)
        struct kevent kev;
        EV_SET(&kev, loop->wakeup_fd, EVFILT_READ, EV_ADD, 0, 0, &loop->wakeup_entry);
        kevent(loop->kq, &kev, 1, NULL, 0, NULL);
#else
        platform_add_fd(loop, loop->wakeup_fd, POLLIN);
#endif
    }
#endif

    return loop;
}

/**
 * @brief Create an event loop using the system allocator.
 *
 * @return New event loop, or NULL on failure.
 */
cobalt_eventloop_t *cobalt_eventloop_create(void)
{
    return cobalt_eventloop_create_with_alloc(cobalt_allocator_get_system());
}

/**
 * @brief Create an event loop using a caller-supplied allocator.
 *
 * @param alloc Allocator used for the loop and all its state.
 * @return New event loop, or NULL on NULL allocator or setup failure.
 */
cobalt_eventloop_t *cobalt_eventloop_create_with_allocator(cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    return cobalt_eventloop_create_with_alloc(alloc);
}

/**
 * @brief Release an event loop and all its descriptors, timers and state.
 *
 * @param loop Loop to destroy; NULL is ignored.
 *
 * @note Invokes the registered close callback before freeing the loop itself.
 */
void cobalt_eventloop_destroy(cobalt_eventloop_t *loop)
{
    if (!loop) {
        return;
    }

#ifdef __linux__
    if (loop->epoll_fd >= 0) {
        close(loop->epoll_fd);
    }
    loop->alloc->free(loop->alloc, loop->epoll_events);
#elif __APPLE__
    if (loop->kq >= 0) {
        close(loop->kq);
    }
#elif defined(_WIN32)
    /* Nothing to free for unsupported Windows port */
#else
    loop->alloc->free(loop->alloc, loop->pollfds);
#endif

    fd_entry_t *fd = loop->fd_head;
    while (fd) {
        fd_entry_t *next = fd->next;
        loop->alloc->free(loop->alloc, fd);
        fd = next;
    }

    /* Free all pending timers in the timing wheel */
    for (int i = 0; i < COBALT_TW_L0_SIZE; i++) {
        cobalt_timer_node_t *node = loop->wheel.slots_l0[i].head;
        while (node) {
            cobalt_timer_node_t *next = node->next;
            loop->alloc->free(loop->alloc, node);
            node = next;
        }
    }
    for (int lvl = 0; lvl < 3; lvl++) {
        for (int i = 0; i < COBALT_TW_LN_SIZE; i++) {
            cobalt_timer_node_t *node = loop->wheel.slots_ln[lvl][i].head;
            while (node) {
                cobalt_timer_node_t *next = node->next;
                loop->alloc->free(loop->alloc, node);
                node = next;
            }
        }
    }
    timer_table_destroy(loop->alloc, &loop->timer_table);

    /* Drain and free remaining posted tasks and destroy post queue */
    if (loop->post_queue) {
        void *item = NULL;
        while (cobalt_lfqueue_dequeue(loop->post_queue, &item) == 0 && item != NULL) {
            loop->alloc->free(loop->alloc, item);
        }
        cobalt_lfqueue_destroy(loop->post_queue);
        loop->post_queue = NULL;
    }

    /* Close wakeup descriptors */
#if defined(__linux__)
    if (loop->wakeup_fd >= 0) {
        epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, loop->wakeup_fd, NULL);
        close(loop->wakeup_fd);
        loop->wakeup_fd = -1;
    }
#elif !defined(_WIN32)
    if (loop->wakeup_fd >= 0) {
        platform_del_fd(loop, loop->wakeup_fd);
        close(loop->wakeup_fd);
        loop->wakeup_fd = -1;
    }
    if (loop->wakeup_write_fd >= 0) {
        close(loop->wakeup_write_fd);
        loop->wakeup_write_fd = -1;
    }
#endif

    /* Invoke close callback */
    if (loop->close_callback) {
        loop->close_callback(-1, 0, loop->close_user_data);
    }

    loop->alloc->free(loop->alloc, loop);
}

/**
 * @brief Register a handler invoked when the process receives a signal.
 *
 * @param loop Unused; signal state is process-global.
 * @param signum Signal number to watch.
 * @param callback Handler invoked with the signal number on delivery.
 * @param user_data Opaque value passed to the handler.
 * @return 0 on success, -1 on invalid arguments, duplicates or a full table.
 */
int cobalt_eventloop_add_signal(cobalt_eventloop_t *loop,
                                int                 signum,
                                fd_handler_t        callback,
                                void               *user_data)
{
    (void)loop;
    if (signum <= 0 || !callback) {
        return -1;
    }
    /* Check for duplicate */
    for (int i = 0; i < COBALT_MAX_SIGNAL_HANDLERS; i++) {
        if (g_signal_table[i].active && g_signal_table[i].signum == signum) {
            return -1;
        }
    }
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < COBALT_MAX_SIGNAL_HANDLERS; i++) {
        if (!g_signal_table[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return -1;
    }
    g_signal_table[slot].signum    = signum;
    g_signal_table[slot].callback  = callback;
    g_signal_table[slot].user_data = user_data;

#ifndef _WIN32
    g_signal_table[slot].active = 1;
    struct sigaction sa         = {};
    sa.sa_handler               = cobalt_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(signum, &sa, NULL) != 0) {
        g_signal_table[slot].active = 0;
        return -1;
    }
#else
    (void)signum;
    (void)callback;
    (void)user_data;
    g_signal_table[slot].active = 0;
    return -1;
#endif
    return 0;
}

/**
 * @brief Set the callback invoked when the loop is destroyed.
 *
 * @param loop Event loop storing the callback.
 * @param callback Handler invoked at destroy time; may be NULL to clear.
 * @param user_data Opaque value passed to the handler.
 * @return 0 on success, -1 on NULL loop.
 */
int cobalt_eventloop_add_close_callback(cobalt_eventloop_t *loop,
                                        fd_handler_t        callback,
                                        void               *user_data)
{
    if (!loop) {
        return -1;
    }
    loop->close_callback  = callback;
    loop->close_user_data = user_data;
    return 0;
}

/**
 * @brief Watch a file descriptor and invoke a handler on activity.
 *
 * @param loop Event loop receiving the descriptor.
 * @param fd Descriptor to watch.
 * @param events Event mask to wait for.
 * @param callback Handler invoked on descriptor activity.
 * @param user_data Opaque value passed to the handler.
 * @return 0 on success, -1 on invalid arguments or backend failure.
 */
int cobalt_eventloop_add_fd(cobalt_eventloop_t *loop,
                            cobalt_fd_t         fd,
                            cobalt_events_t     events,
                            fd_handler_t        callback,
                            void               *user_data)
{
    if (!loop || fd < 0 || !callback) {
        return -1;
    }
    {
        const fd_entry_t *entry = loop->fd_head;
        while (entry) {
            if (entry->fd == fd) {
                return -1;
            }
            entry = entry->next;
        }
    }
    if (platform_add_fd(loop, (int)fd, (short)events) < 0) {
        return -1;
    }

    fd_entry_t *entry = (fd_entry_t *)loop->alloc->alloc(loop->alloc, sizeof(fd_entry_t));
    if (!entry) {
        return -1;
    }
    memset(entry, 0, sizeof(fd_entry_t));
    entry->fd        = (int)fd;
    entry->events    = (short)events;
    entry->callback  = callback;
    entry->user_data = user_data;

#ifdef __linux__
    {
        struct epoll_event ev = {0};
        ev.events             = events;
        ev.data.ptr           = entry;
        epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, (int)fd, &ev);
    }
#endif

    if (!loop->fd_head) {
        loop->fd_head = loop->fd_tail = entry;
    } else {
        loop->fd_tail->next = entry;
        loop->fd_tail       = entry;
    }
    return 0;
}

/**
 * @brief Replace the watch on a descriptor by re-adding it.
 *
 * @param loop Event loop owning the watch.
 * @param fd Descriptor to re-register.
 * @param events New event mask to wait for.
 * @param callback New handler invoked on descriptor activity.
 * @param user_data Opaque value passed to the handler.
 * @return 0 on success, -1 on invalid arguments or backend failure.
 */
int cobalt_eventloop_mod_fd(cobalt_eventloop_t *loop,
                            cobalt_fd_t         fd,
                            cobalt_events_t     events,
                            fd_handler_t        callback,
                            void               *user_data)
{
    if (!loop || fd < 0) {
        return -1;
    }
    cobalt_eventloop_del_fd(loop, fd);
    return cobalt_eventloop_add_fd(loop, fd, events, callback, user_data);
}

/**
 * @brief Stop watching a descriptor and release its loop state.
 *
 * @param loop Event loop owning the watch.
 * @param fd Descriptor to stop watching.
 * @return 0 on success, -1 when the descriptor is not watched.
 */
int cobalt_eventloop_del_fd(cobalt_eventloop_t *loop, cobalt_fd_t fd)
{
    if (!loop || fd < 0) {
        return -1;
    }
    platform_del_fd(loop, (int)fd);

    fd_entry_t **prev = &loop->fd_head;
    fd_entry_t  *curr = *prev;
    fd_entry_t  *last = NULL;
    while (curr) {
        if (curr->fd == (int)fd) {
            *prev = curr->next;
            if (loop->fd_tail == curr) {
                loop->fd_tail = last;
            }
            loop->alloc->free(loop->alloc, curr);
            return 0;
        }
        last = curr;
        prev = &curr->next;
        curr = curr->next;
    }
    return -1;
}

/**
 * @brief Arm a one-shot or repeating timer on the loop.
 *
 * @param loop Event loop owning the timing wheel.
 * @param timeout_ms Delay before the first firing.
 * @param interval_ms Repeat period; zero makes the timer one-shot.
 * @param callback Handler invoked with the timer id on each firing.
 * @param user_data Opaque value passed to the handler.
 * @return Nonzero timer id on success, 0 on invalid arguments or failure.
 */
uint64_t cobalt_eventloop_add_timer(cobalt_eventloop_t  *loop,
                                    cobalt_timeout_ms_t  timeout_ms,
                                    cobalt_interval_ms_t interval_ms,
                                    timer_handler_t      callback,
                                    void                *user_data)
{
    if (!loop || !callback) {
        return 0;
    }

    uint64_t timer_id = ++loop->next_timer_id;
    if (timer_id == 0) {
        timer_id = ++loop->next_timer_id;
    }

    cobalt_timer_node_t *node =
        (cobalt_timer_node_t *)loop->alloc->alloc(loop->alloc, sizeof(cobalt_timer_node_t));
    if (!node) {
        return 0;
    }
    memset(node, 0, sizeof(cobalt_timer_node_t));

    node->timer_id    = timer_id;
    node->interval_ms = interval_ms;
    node->callback    = callback;
    node->user_data   = user_data;
    node->active      = true;
    node->firing      = false;
    node->deleted     = false;

    struct timespec ts;
    get_time_now(&ts);
    uint64_t now_ms = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;

    uint64_t base_time =
        (now_ms > loop->wheel.wheel_time_ms) ? now_ms : (loop->wheel.wheel_time_ms + 1);
    node->expire_time_ms = base_time + timeout_ms;

    if (timer_table_insert(loop, &loop->timer_table, timer_id, node) != 0) {
        loop->alloc->free(loop->alloc, node);
        return 0;
    }

    wheel_insert_node(loop, node);
    loop->timer_count++;
    return timer_id;
}

/**
 * @brief Cancel a timer and release its entry in O(1) time.
 *
 * @param loop Event loop owning the timing wheel.
 * @param timer_id Id returned by add_timer.
 * @return 0 on success, -1 when the id is not found.
 */
int cobalt_eventloop_del_timer(cobalt_eventloop_t *loop, uint64_t timer_id)
{
    if (!loop || timer_id == 0) {
        return -1;
    }

    cobalt_timer_node_t *node = timer_table_find(&loop->timer_table, timer_id);
    if (!node) {
        return -1;
    }

    timer_table_remove(&loop->timer_table, timer_id);
    loop->timer_count--;

    if (node->firing) {
        node->deleted = true;
        node->active  = false;
        return 0;
    }

    timer_slot_t *s = wheel_get_slot(&loop->wheel, node->level, node->slot);
    if (s) {
        slot_remove(s, node);
    }
    loop->alloc->free(loop->alloc, node);
    return 0;
}

/**
 * @brief Run the loop until stop is requested.
 *
 * @param loop Event loop to run; NULL is ignored.
 */
void cobalt_eventloop_run(cobalt_eventloop_t *loop)
{
    if (!loop) {
        return;
    }
    loop->running   = 1;
    loop->stop_flag = 0;
    while (loop->running && !loop->stop_flag) {
        cobalt_eventloop_iteration(loop);
    }
    loop->running = 0;
}

/**
 * @brief Request a running loop to exit after the current iteration.
 *
 * @param loop Event loop to stop; NULL is ignored.
 *
 * @note Safe to call from within a loop callback.
 */
void cobalt_eventloop_stop(cobalt_eventloop_t *loop)
{
    if (loop) {
        loop->stop_flag = 1;
    }
}

/**
 * @brief Fire due timers, dispatch signals and wait for one backend event batch.
 *
 * @param loop Event loop to drive forward.
 * @return 0 on success, -1 on NULL loop.
 */
int cobalt_eventloop_iteration(cobalt_eventloop_t *loop)
{
    if (!loop) {
        return -1;
    }

    struct timespec now;
    get_time_now(&now);
    uint64_t now_ms = (uint64_t)now.tv_sec * 1000ULL + (uint64_t)now.tv_nsec / 1000000ULL;
    wheel_advance(loop, now_ms);
    cobalt_drain_signals(loop);

    /* Drain any posted tasks before possibly sleeping */
    drain_post_queue(loop);

    /* Mark as sleeping before platform_wait */
    atomic_store_explicit(&loop->is_sleeping, true, memory_order_seq_cst);
    /* Re-check if tasks arrived right before setting is_sleeping */
    if (!cobalt_lfqueue_is_empty(loop->post_queue)) {
        atomic_store_explicit(&loop->is_sleeping, false, memory_order_relaxed);
        drain_post_queue(loop);
    }

    int   event_count = 0;
    void *entries     = NULL;
    platform_wait(loop, &event_count, &entries, 0);

    /* Loop thread has unblocked */
    atomic_store_explicit(&loop->is_sleeping, false, memory_order_relaxed);

#ifdef __linux__
    for (int i = 0; i < event_count; i++) {
        struct epoll_event *ev    = (struct epoll_event *)entries;
        fd_entry_t         *entry = (fd_entry_t *)ev[i].data.ptr;
        if (entry && entry->callback) {
            entry->callback(entry->fd, (short)ev[i].events, entry->user_data);
        }
    }
#elif __APPLE__
    struct kevent *events = (struct kevent *)entries;
    for (int i = 0; i < event_count; i++) {
        int fd = (int)events[i].ident;
        if (fd == loop->wakeup_fd) {
            on_wakeup_event(fd, 0, loop);
            continue;
        }
        fd_entry_t *entry = loop->fd_head;
        while (entry && entry->fd != fd) {
            entry = entry->next;
        }
        if (entry && entry->callback) {
            short ev = (events[i].filter == EVFILT_READ) ? EVFILT_READ : EVFILT_WRITE;
            entry->callback(fd, ev, entry->user_data);
        }
    }
#elif defined(_WIN32)
    (void)loop;
    (void)event_count;
    (void)entries;
#else
    struct pollfd *pfds = (struct pollfd *)entries;
    for (int i = 0; i < event_count; i++) {
        if (pfds[i].revents == 0) {
            continue;
        }
        int fd = pfds[i].fd;
        if (fd == loop->wakeup_fd) {
            on_wakeup_event(fd, 0, loop);
            continue;
        }
        fd_entry_t *entry = loop->fd_head;
        while (entry && entry->fd != fd) {
            entry = entry->next;
        }
        if (entry && entry->callback) {
            short revents = (short)pfds[i].revents;
            entry->callback(fd, revents, entry->user_data);
        }
    }
#endif

    /* Drain posted tasks after I/O callbacks */
    drain_post_queue(loop);

    get_time_now(&now);
    now_ms = (uint64_t)now.tv_sec * 1000ULL + (uint64_t)now.tv_nsec / 1000000ULL;
    wheel_advance(loop, now_ms);

    return 0;
}

/**
 * @brief Post an asynchronous task to be executed on the event loop thread
 *
 * @param loop Event loop object
 * @param fn Callback function to execute
 * @param arg User data passed to callback function
 * @return 0 on success, -1 on invalid arguments or queue failure
 */
int cobalt_eventloop_post(cobalt_eventloop_t *loop, cobalt_loop_task_fn_t fn, void *arg)
{
    if (!loop || !fn || !loop->post_queue) {
        return -1;
    }

    cobalt_post_task_t *task =
        (cobalt_post_task_t *)loop->alloc->alloc(loop->alloc, sizeof(cobalt_post_task_t));
    if (!task) {
        return -1;
    }
    task->fn  = fn;
    task->arg = arg;

    int ret = -1;
    for (int retry = 0; retry < 1000; retry++) {
        ret = cobalt_lfqueue_enqueue(loop->post_queue, task);
        if (ret == 0) {
            break;
        }
        cobalt_thread_yield();
    }
    if (ret != 0) {
        loop->alloc->free(loop->alloc, task);
        return -1;
    }

    /* Syscall coalescing: only issue kernel write if loop thread is sleeping */
    if (atomic_exchange_explicit(&loop->is_sleeping, false, memory_order_seq_cst)) {
        wakeup_loop(loop);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* Asynchronous Stream Abstraction Implementation                              */
/* -------------------------------------------------------------------------- */

struct cobalt_stream {
    cobalt_eventloop_t             *loop;
    cobalt_fd_t                     fd;
    void                           *user_data;
    cobalt_buffer_t                *input_buffer;
    cobalt_buffer_t                *output_buffer;
    cobalt_stream_read_fn           on_read;
    cobalt_stream_write_complete_fn on_write_complete;
    cobalt_stream_error_fn          on_error;
    bool                            writing;
};

static void stream_fd_event_cb(cobalt_fd_t fd, cobalt_events_t events, void *user_data)
{
    cobalt_stream_t *stream = (cobalt_stream_t *)user_data;
    if (!stream) {
        return;
    }

    bool closed = false;

    /* 1. Handle incoming data (POLLIN) */
    if (events & POLLIN) {
        int     err = 0;
        ssize_t n   = cobalt_buffer_read_fd(stream->input_buffer, fd, &err);
        if (n > 0) {
            if (stream->on_read) {
                stream->on_read(stream, stream->input_buffer, stream->user_data);
            }
        } else if (n == 0) {
            closed = true;
            if (stream->on_error) {
                stream->on_error(stream, 0, stream->user_data);
            }
            return;
        } else {
            if (err != EAGAIN && err != EWOULDBLOCK && err != EINTR) {
                closed = true;
                if (stream->on_error) {
                    stream->on_error(stream, err, stream->user_data);
                }
                return;
            }
        }
    }

    /* 2. Handle outgoing data (POLLOUT) */
    if (!closed && (events & POLLOUT)) {
        if (cobalt_buffer_readable_bytes(stream->output_buffer) > 0) {
            int     err = 0;
            ssize_t nw  = cobalt_buffer_write_fd(stream->output_buffer, fd, &err);
            if (nw >= 0) {
                if (cobalt_buffer_readable_bytes(stream->output_buffer) == 0) {
                    stream->writing = false;
                    cobalt_eventloop_mod_fd(stream->loop, fd, POLLIN, stream_fd_event_cb, stream);
                    if (stream->on_write_complete) {
                        stream->on_write_complete(stream, stream->user_data);
                    }
                }
            } else {
                if (err != EAGAIN && err != EWOULDBLOCK && err != EINTR) {
                    if (stream->on_error) {
                        stream->on_error(stream, err, stream->user_data);
                    }
                    return;
                }
            }
        } else {
            if (stream->writing) {
                stream->writing = false;
                cobalt_eventloop_mod_fd(stream->loop, fd, POLLIN, stream_fd_event_cb, stream);
            }
        }
    }

    /* 3. Handle errors */
    if (!closed && (events & (POLLERR | POLLNVAL))) {
        if (stream->on_error) {
            stream->on_error(stream, EIO, stream->user_data);
        }
    }
}

cobalt_stream_t *cobalt_stream_create(cobalt_eventloop_t *loop, cobalt_fd_t fd, void *user_data)
{
    if (!loop || fd < 0) {
        return NULL;
    }

#ifndef _WIN32
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
#endif

    cobalt_stream_t *stream =
        (cobalt_stream_t *)loop->alloc->alloc(loop->alloc, sizeof(cobalt_stream_t));
    if (!stream) {
        return NULL;
    }
    memset(stream, 0, sizeof(*stream));
    stream->loop      = loop;
    stream->fd        = fd;
    stream->user_data = user_data;

    stream->input_buffer = cobalt_buffer_create_with_alloc(
        COBALT_BUFFER_DEFAULT_INITIAL_CAPACITY, COBALT_BUFFER_DEFAULT_PREPEND_SIZE, loop->alloc);
    stream->output_buffer = cobalt_buffer_create_with_alloc(
        COBALT_BUFFER_DEFAULT_INITIAL_CAPACITY, COBALT_BUFFER_DEFAULT_PREPEND_SIZE, loop->alloc);
    if (!stream->input_buffer || !stream->output_buffer) {
        if (stream->input_buffer) {
            cobalt_buffer_destroy(stream->input_buffer);
        }
        if (stream->output_buffer) {
            cobalt_buffer_destroy(stream->output_buffer);
        }
        loop->alloc->free(loop->alloc, stream);
        return NULL;
    }

    if (cobalt_eventloop_add_fd(loop, fd, POLLIN, stream_fd_event_cb, stream) != 0) {
        cobalt_buffer_destroy(stream->input_buffer);
        cobalt_buffer_destroy(stream->output_buffer);
        loop->alloc->free(loop->alloc, stream);
        return NULL;
    }

    return stream;
}

void cobalt_stream_destroy(cobalt_stream_t *stream)
{
    if (!stream) {
        return;
    }
    cobalt_eventloop_del_fd(stream->loop, stream->fd);
    if (stream->input_buffer) {
        cobalt_buffer_destroy(stream->input_buffer);
        stream->input_buffer = NULL;
    }
    if (stream->output_buffer) {
        cobalt_buffer_destroy(stream->output_buffer);
        stream->output_buffer = NULL;
    }
    cobalt_allocator_t *alloc = stream->loop->alloc;
    alloc->free(alloc, stream);
}

void cobalt_stream_set_callbacks(cobalt_stream_t                *stream,
                                 cobalt_stream_read_fn           on_read,
                                 cobalt_stream_write_complete_fn on_write_complete,
                                 cobalt_stream_error_fn          on_error)
{
    if (!stream) {
        return;
    }
    stream->on_read           = on_read;
    stream->on_write_complete = on_write_complete;
    stream->on_error          = on_error;
}

int cobalt_stream_send(cobalt_stream_t *stream, const void *data, size_t len)
{
    if (!stream || !data) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    size_t pending = cobalt_buffer_readable_bytes(stream->output_buffer);
    if (pending == 0) {
        ssize_t n = write(stream->fd, data, len);
        if (n >= 0) {
            size_t written = (size_t)n;
            if (written == len) {
                return 0;
            }
            int ret = cobalt_buffer_append(
                stream->output_buffer, (const char *)data + written, len - written);
            if (ret != 0) {
                return -1;
            }
            if (!stream->writing) {
                stream->writing = true;
                cobalt_eventloop_mod_fd(
                    stream->loop, stream->fd, POLLIN | POLLOUT, stream_fd_event_cb, stream);
            }
            return 0;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                int ret = cobalt_buffer_append(stream->output_buffer, data, len);
                if (ret != 0) {
                    return -1;
                }
                if (!stream->writing) {
                    stream->writing = true;
                    cobalt_eventloop_mod_fd(
                        stream->loop, stream->fd, POLLIN | POLLOUT, stream_fd_event_cb, stream);
                }
                return 0;
            } else {
                if (stream->on_error) {
                    stream->on_error(stream, errno, stream->user_data);
                }
                return -1;
            }
        }
    } else {
        int ret = cobalt_buffer_append(stream->output_buffer, data, len);
        if (ret != 0) {
            return -1;
        }
        if (!stream->writing) {
            stream->writing = true;
            cobalt_eventloop_mod_fd(
                stream->loop, stream->fd, POLLIN | POLLOUT, stream_fd_event_cb, stream);
        }
        return 0;
    }
}

int cobalt_stream_send_buffer(cobalt_stream_t *stream, cobalt_buffer_t *buf)
{
    if (!stream || !buf) {
        return -1;
    }
    size_t len = cobalt_buffer_readable_bytes(buf);
    if (len == 0) {
        return 0;
    }
    int ret = cobalt_stream_send(stream, cobalt_buffer_peek(buf), len);
    if (ret == 0) {
        cobalt_buffer_retrieve_all(buf);
    }
    return ret;
}

cobalt_buffer_t *cobalt_stream_get_input_buffer(cobalt_stream_t *stream)
{
    return stream ? stream->input_buffer : NULL;
}

cobalt_buffer_t *cobalt_stream_get_output_buffer(cobalt_stream_t *stream)
{
    return stream ? stream->output_buffer : NULL;
}
