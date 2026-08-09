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
typedef struct timer_entry {
    uint64_t        timer_id;
    uint64_t        timeout_ms;
    uint64_t        interval_ms;
    timer_handler_t callback;
    void           *user_data;
    int             active;
    struct timespec next_fire;
} timer_entry_t;

typedef struct fd_entry {
    int              fd;
    short            events;
    fd_handler_t     callback;
    void            *user_data;
    struct fd_entry *next;
} fd_entry_t;

/* Signal ring buffer — async-signal-safe, fixed size */
static int                   g_signal_ring[COBALT_SIGNAL_RING_SIZE];
static volatile sig_atomic_t g_signal_head  = 0;
static volatile sig_atomic_t g_signal_tail  = 0;
static int                   g_signal_count = 0;

/* Signal handler table (signum -> callback + user_data) */
typedef struct {
    fd_handler_t callback;
    void        *user_data;
    int          signum;
    int          active;
} signal_handler_entry_t;

#define COBALT_MAX_SIGNAL_HANDLERS 32
static signal_handler_entry_t g_signal_table[COBALT_MAX_SIGNAL_HANDLERS];

struct cobalt_eventloop {
#ifdef __linux__
    int                 epoll_fd;
    struct epoll_event *epoll_events;
    int                 epoll_capacity;
#elif __APPLE__
    int kq;
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

static timer_entry_t *heap_peek(const cobalt_eventloop_t *loop)
{
    return loop->timer_count > 0 ? loop->timer_heap[0] : NULL;
}

static void get_time_now(struct timespec *ts)
{
    clock_gettime(CLOCK_MONOTONIC, ts);
}

static int timer_expired(const struct timespec *a, const struct timespec *b)
{
    long secs  = a->tv_sec - b->tv_sec;
    long nsecs = a->tv_nsec - b->tv_nsec;
    return secs < 0 || (secs == 0 && nsecs <= 0);
}

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

static int platform_mod_fd(cobalt_eventloop_t *loop, int fd, short events)
{
#ifdef __linux__
    struct epoll_event ev = {0};
    ev.events             = events;
    ev.data.ptr           = NULL;
    return epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
#elif __APPLE__
    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_CHANGE, 0, 0, NULL);
    return kevent(loop->kq, &kev, 1, NULL, 0, NULL);
#else
    if (fd < 0 || fd >= loop->pollfds_capacity) {
        return -1;
    }
    loop->pollfds[fd].events = events;
    return 0;
#endif
}

static int platform_del_fd(cobalt_eventloop_t *loop, int fd)
{
#ifdef __linux__
    return epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
#elif __APPLE__
    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    return kevent(loop->kq, &kev, 1, NULL, 0, NULL);
#else
    if (fd < 0 || fd >= loop->pollfds_capacity) {
        return 0;
    }
    loop->pollfds[fd].fd     = -1;
    loop->pollfds[fd].events = 0;
    return 0;
#endif
}

static void
platform_wait(cobalt_eventloop_t *loop, int *event_count_out, void **entries_out, int max_events)
{
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
}

int cobalt_eventloop_accept(cobalt_eventloop_t *loop,
                            cobalt_fd_t         listen_fd,
                            cobalt_events_t     events,
                            fd_handler_t        callback,
                            void               *user_data)
{
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
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */
static cobalt_eventloop_t *cobalt_eventloop_create_with_alloc(cobalt_allocator_t *alloc)
{
    cobalt_eventloop_t *loop =
        (cobalt_eventloop_t *)alloc->alloc(alloc, sizeof(cobalt_eventloop_t));
    if (!loop) {
        return NULL;
    }
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

cobalt_eventloop_t *cobalt_eventloop_create(void)
{
    return cobalt_eventloop_create_with_alloc(cobalt_allocator_get_system());
}

cobalt_eventloop_t *cobalt_eventloop_create_with_allocator(cobalt_allocator_t *alloc)
{
    if (!alloc) {
        return NULL;
    }
    return cobalt_eventloop_create_with_alloc(alloc);
}

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
    g_signal_table[slot].active    = 1;

    struct sigaction sa = {};
    sa.sa_handler       = cobalt_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(signum, &sa, NULL) != 0) {
        g_signal_table[slot].active = 0;
        return -1;
    }
    return 0;
}

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

int cobalt_eventloop_add_fd(cobalt_eventloop_t *loop,
                            cobalt_fd_t         fd,
                            cobalt_events_t     events,
                            fd_handler_t        callback,
                            void               *user_data)
{
    if (!loop || fd < 0 || !callback) {
        return -1;
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

void cobalt_eventloop_stop(cobalt_eventloop_t *loop)
{
    if (loop) {
        loop->stop_flag = 1;
    }
}

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
            short ev = (events[i].filter == EVFILT_READ) ? POLLIN : POLLOUT;
            entry->callback(fd, ev, entry->user_data);
        }
    }
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
