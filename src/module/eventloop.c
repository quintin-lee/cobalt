/**
 * @file eventloop.c
 * @brief Implementation of the event-driven I/O loop module
 */
#define _POSIX_C_SOURCE 200112L
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "cobalt/module/eventloop.h"
#include "cobalt/memory/allocator.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#elif __APPLE__
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/un.h>
#elif defined(_WIN32)
/* MinGW does not provide poll.h; Windows backend is not supported. */
#else
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

#define COBALT_EVENTLOOP_DEFAULT_CAPACITY 16
#define COBALT_EVENTLOOP_TIMEOUT_MS 10
#define COBALT_EVENTLOOP_TIMER_HEAP_INITIAL 16
#define COBALT_SIGNAL_RING_SIZE 64
#define COBALT_MILLIS_PER_SEC 1000ULL
#define COBALT_NANOS_PER_MILLI 1000000ULL
#define COBALT_NANOS_PER_SEC 1000000000LL

/* -------------------------------------------------------------------------- */
/* Internal types                                                             */
/* -------------------------------------------------------------------------- */
/**
 * @brief Timer entry stored in the expiry min-heap
 */
typedef struct timer_entry {
    uint64_t        timer_id;    /**< Unique id returned to the caller */
    uint64_t        timeout_ms;  /**< Initial delay before first fire */
    uint64_t        interval_ms; /**< Repeat interval (0 for one-shot) */
    timer_handler_t callback;    /**< Function invoked on expiry */
    void           *user_data;   /**< Opaque callback argument */
    int             active;      /**< Non-zero while the timer is armed */
    struct timespec next_fire;   /**< Absolute monotonic time of next fire */
} timer_entry_t;

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
static int                   g_signal_count = 0;

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
 *          the timer min-heap, and the signal dispatch table.
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
    fd_entry_t     *fd_head;
    fd_entry_t     *fd_tail;
    timer_entry_t **timer_heap;
    int             timer_count;
    int             timer_capacity;
    uint64_t        next_timer_id;
    int             running;
    int             stop_flag;
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

/* -------------------------------------------------------------------------- */
/* Heap operations                                                            */
/* -------------------------------------------------------------------------- */
/**
 * @brief Order two timers by fire time, breaking ties by timer id.
 *
 * @param a First timer entry.
 * @param b Second timer entry.
 * @return Negative if a fires first, positive if b fires first, zero if equal.
 */
static int timer_compare(const timer_entry_t *a, const timer_entry_t *b)
{
    if (a->next_fire.tv_sec < b->next_fire.tv_sec) {
        return -1;
    }
    if (a->next_fire.tv_sec > b->next_fire.tv_sec) {
        return 1;
    }
    if (a->next_fire.tv_nsec < b->next_fire.tv_nsec) {
        return -1;
    }
    if (a->next_fire.tv_nsec > b->next_fire.tv_nsec) {
        return 1;
    }
    /* Tiebreak by timer_id for FIFO ordering of same-time timers */
    if (a->timer_id < b->timer_id) {
        return -1;
    }
    return a->timer_id > b->timer_id ? 1 : 0;
}

/**
 * @brief Restore the heap invariant by moving an entry up toward the root.
 *
 * @param heap Timer heap array.
 * @param idx Index of the entry to sift up.
 */
static void heap_sift_up(timer_entry_t **heap, int idx)
{
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (timer_compare(heap[parent], heap[idx]) <= 0) {
            break;
        }
        timer_entry_t *tmp = heap[parent];
        heap[parent]       = heap[idx];
        heap[idx]          = tmp;
        idx                = parent;
    }
}

/**
 * @brief Restore the heap invariant by moving an entry down toward the leaves.
 *
 * @param heap Timer heap array.
 * @param count Number of entries in the heap.
 * @param idx Index of the entry to sift down.
 */
static void heap_sift_down(timer_entry_t **heap, int count, int idx)
{
    while (1) {
        int smallest = idx;
        int left     = 2 * idx + 1;
        int right    = 2 * idx + 2;
        if (left < count && timer_compare(heap[left], heap[smallest]) < 0) {
            smallest = left;
        }
        if (right < count && timer_compare(heap[right], heap[smallest]) < 0) {
            smallest = right;
        }
        if (smallest == idx) {
            break;
        }
        timer_entry_t *tmp = heap[idx];
        heap[idx]          = heap[smallest];
        heap[smallest]     = tmp;
        idx                = smallest;
    }
}

/**
 * @brief Insert a timer entry into the loop timer heap, growing it as needed.
 *
 * @param loop Event loop owning the timer heap.
 * @param entry Timer entry to insert.
 * @return 0 on success, -1 when heap growth fails.
 */
static int heap_push(cobalt_eventloop_t *loop, timer_entry_t *entry)
{
    if (loop->timer_count >= loop->timer_capacity) {
        int             new_cap  = loop->timer_capacity * 2;
        timer_entry_t **new_heap = (timer_entry_t **)loop->alloc->realloc(
            loop->alloc, loop->timer_heap, sizeof(timer_entry_t *) * new_cap);
        if (!new_heap) {
            return -1;
        }
        loop->timer_heap     = new_heap;
        loop->timer_capacity = new_cap;
    }
    loop->timer_heap[loop->timer_count] = entry;
    heap_sift_up(loop->timer_heap, loop->timer_count);
    loop->timer_count++;
    return 0;
}

/**
 * @brief Remove and return the earliest-firing timer from the heap.
 *
 * @param loop Event loop owning the timer heap.
 * @return Earliest timer entry, or NULL when the heap is empty.
 */
static timer_entry_t *heap_pop_min(cobalt_eventloop_t *loop)
{
    if (loop->timer_count == 0) {
        return NULL;
    }
    timer_entry_t *min = loop->timer_heap[0];
    loop->timer_count--;
    loop->timer_heap[0] = loop->timer_heap[loop->timer_count];
    heap_sift_down(loop->timer_heap, loop->timer_count, 0);
    return min;
}

/**
 * @brief Return the earliest-firing timer without removing it.
 *
 * @param loop Event loop owning the timer heap.
 * @return Earliest timer entry, or NULL when the heap is empty.
 */
static timer_entry_t *heap_peek(const cobalt_eventloop_t *loop)
{
    return loop->timer_count > 0 ? loop->timer_heap[0] : NULL;
}

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
 * @brief Test whether fire time a has been reached relative to time b.
 *
 * @param a Timer fire time.
 * @param b Reference time.
 * @return Nonzero when a is at or before b, zero otherwise.
 */
static int timer_expired(const struct timespec *a, const struct timespec *b)
{
    long secs  = a->tv_sec - b->tv_sec;
    long nsecs = a->tv_nsec - b->tv_nsec;
    return secs < 0 || (secs == 0 && nsecs <= 0);
}

/**
 * @brief Fire due timers and requeue repeating ones.
 *
 * @param loop Event loop owning the timer heap.
 * @param now Current time used to decide which timers are due.
 */
static void process_expired_timers(cobalt_eventloop_t *loop, const struct timespec *now)
{
    while (loop->timer_count > 0) {
        timer_entry_t *timer = heap_peek(loop);
        if (!timer || !timer->active || !timer_expired(&timer->next_fire, now)) {
            break;
        }

        heap_pop_min(loop);

        if (timer->callback) {
            timer->callback(timer->timer_id, timer->user_data);
        }
        if (timer->interval_ms > 0) {
            timer->next_fire.tv_sec += (long)(timer->interval_ms / COBALT_MILLIS_PER_SEC);
            timer->next_fire.tv_nsec +=
                (long)((timer->interval_ms % COBALT_MILLIS_PER_SEC) * COBALT_NANOS_PER_MILLI);
            if (timer->next_fire.tv_nsec >= COBALT_NANOS_PER_SEC) {
                timer->next_fire.tv_sec++;
                timer->next_fire.tv_nsec -= COBALT_NANOS_PER_SEC;
            }
            heap_push(loop, timer);
        } else {
            loop->alloc->free(loop->alloc, timer);
        }
    }
}

/**
 * @brief Compute how long the backend may block before the next timer fires.
 *
 * @param loop Event loop owning the timer heap.
 * @param now Current time, or NULL to use the default timeout.
 * @return Milliseconds to wait; never negative.
 */
static int calculate_timeout_ms(const cobalt_eventloop_t *loop, const struct timespec *now)
{
    int                  timeout_ms = COBALT_EVENTLOOP_TIMEOUT_MS;
    const timer_entry_t *next       = heap_peek(loop);
    if (next && now) {
        long secs  = next->next_fire.tv_sec - now->tv_sec;
        long nsecs = next->next_fire.tv_nsec - now->tv_nsec;
        timeout_ms = (int)((secs * COBALT_MILLIS_PER_SEC) + (nsecs / COBALT_NANOS_PER_MILLI));
        if (timeout_ms < 0) {
            timeout_ms = 0;
        }
    }
    return timeout_ms;
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
        int            new_cap  = loop->pollfds_capacity == 0 ? COBALT_EVENTLOOP_DEFAULT_CAPACITY
                                                              : loop->pollfds_capacity * 2;
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

    loop->timer_capacity = COBALT_EVENTLOOP_TIMER_HEAP_INITIAL;
    loop->timer_heap =
        (timer_entry_t **)alloc->alloc(alloc, sizeof(timer_entry_t *) * loop->timer_capacity);
    if (!loop->timer_heap) {
#ifdef __linux__
        close(loop->epoll_fd);
#elif __APPLE__
        close(loop->kq);
#endif
        loop->alloc->free(loop->alloc, loop);
        return NULL;
    }

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

    for (int i = 0; i < loop->timer_count; i++) {
        loop->alloc->free(loop->alloc, loop->timer_heap[i]);
    }
    loop->alloc->free(loop->alloc, loop->timer_heap);

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
    while (curr) {
        if (curr->fd == (int)fd) {
            *prev = curr->next;
            loop->alloc->free(loop->alloc, curr);
            return 0;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    return -1;
}

/**
 * @brief Arm a one-shot or repeating timer on the loop.
 *
 * @param loop Event loop owning the timer heap.
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

    uint64_t       timer_id = ++loop->next_timer_id;
    timer_entry_t *entry = (timer_entry_t *)loop->alloc->alloc(loop->alloc, sizeof(timer_entry_t));
    if (!entry) {
        return 0;
    }
    memset(entry, 0, sizeof(timer_entry_t));

    entry->timer_id    = timer_id;
    entry->timeout_ms  = timeout_ms;
    entry->interval_ms = interval_ms;
    entry->callback    = callback;
    entry->user_data   = user_data;
    entry->active      = 1;

    struct timespec now;
    get_time_now(&now);
    entry->next_fire.tv_sec = now.tv_sec + (long)(timeout_ms / COBALT_MILLIS_PER_SEC);
    entry->next_fire.tv_nsec =
        (long)(now.tv_nsec + ((timeout_ms % COBALT_MILLIS_PER_SEC) * COBALT_NANOS_PER_MILLI));
    if (entry->next_fire.tv_nsec >= COBALT_NANOS_PER_SEC) {
        entry->next_fire.tv_sec++;
        entry->next_fire.tv_nsec -= COBALT_NANOS_PER_SEC;
    }

    if (heap_push(loop, entry) != 0) {
        loop->alloc->free(loop->alloc, entry);
        return 0;
    }
    return timer_id;
}

/**
 * @brief Cancel a timer and release its entry.
 *
 * @param loop Event loop owning the timer heap.
 * @param timer_id Id returned by add_timer.
 * @return 0 on success, -1 when the id is not found.
 */
int cobalt_eventloop_del_timer(cobalt_eventloop_t *loop, uint64_t timer_id)
{
    if (!loop) {
        return -1;
    }
    for (int i = 0; i < loop->timer_count; i++) {
        if (loop->timer_heap[i]->timer_id == timer_id) {
            loop->timer_count--;
            loop->timer_heap[i] = loop->timer_heap[loop->timer_count];
            heap_sift_down(loop->timer_heap, loop->timer_count, i);
            loop->alloc->free(loop->alloc, loop->timer_heap[loop->timer_count]);
            return 0;
        }
    }
    return -1;
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
    process_expired_timers(loop, &now);
    cobalt_drain_signals(loop);

    int   event_count = 0;
    void *entries     = NULL;
    platform_wait(loop, &event_count, &entries, 0);

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
        int         fd    = (int)events[i].ident;
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
        int         fd    = pfds[i].fd;
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

    return 0;
}
