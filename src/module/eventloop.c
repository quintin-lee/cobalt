#define _POSIX_C_SOURCE 200112L
#define _POSIX_C_SOURCE 200112L
#include "cobalt/module/eventloop.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/epoll.h>
#elif __APPLE__
#include <sys/event.h>
#endif

/* Internal timer entry */
typedef struct timer_entry
{
    uint64_t id;
    uint64_t timeout_ms;
    uint64_t interval_ms;
    timer_handler_t cb;
    void* user_data;
    int active;
    struct timespec next_fire;
    struct timer_entry* next;
} timer_entry_t;

/* Internal fd entry */
typedef struct fd_entry
{
    int fd;
    short events;
    fd_handler_t cb;
    void* user_data;
    struct fd_entry* next;
} fd_entry_t;

/* Event loop context */
struct cobalt_eventloop
{
#ifdef __linux__
    int epoll_fd;
    struct epoll_event* epoll_events;
    int epoll_capacity;
#elif __APPLE__
    int kq;
#endif
    fd_entry_t* fd_head;
    fd_entry_t* fd_tail;
    timer_entry_t* timer_head;
    uint64_t next_timer_id;
    int running;
    int stop_flag;
};

/* ============================================================
   HELPERS
   ============================================================ */

static void get_time_now(struct timespec* ts)
{
    clock_gettime(CLOCK_MONOTONIC, ts);
}

static int timer_expired(const struct timespec* a, const struct timespec* b)
{
    long secs = a->tv_sec - b->tv_sec;
    long nsecs = a->tv_nsec - b->tv_nsec;
    return (secs > 0) || (secs == 0 && nsecs >= 0);
}

/* ============================================================
   PUBLIC API
   ============================================================ */

cobalt_eventloop_t* cobalt_eventloop_create(void)
{
    cobalt_eventloop_t* loop = calloc(1, sizeof(cobalt_eventloop_t));
    if (!loop)
        return NULL;

#ifdef __linux__
    loop->epoll_fd = epoll_create1(0);
    if (loop->epoll_fd < 0)
        {
            free(loop);
            return NULL;
        }
    loop->epoll_capacity = 16;
    loop->epoll_events = malloc(sizeof(struct epoll_event) * loop->epoll_capacity);
    if (!loop->epoll_events)
        {
            close(loop->epoll_fd);
            free(loop);
            return NULL;
        }
#elif __APPLE__
    loop->kq = kqueue();
    if (loop->kq < 0)
        {
            free(loop);
            return NULL;
        }
#endif

    return loop;
}

void cobalt_eventloop_destroy(cobalt_eventloop_t* loop)
{
    if (!loop)
        return;

#ifdef __linux__
    if (loop->epoll_fd >= 0)
        close(loop->epoll_fd);
    free(loop->epoll_events);
#elif __APPLE__
    if (loop->kq >= 0)
        close(loop->kq);
#endif

    fd_entry_t* f = loop->fd_head;
    while (f)
        {
            fd_entry_t* next = f->next;
            free(f);
            f = next;
        }

    timer_entry_t* t = loop->timer_head;
    while (t)
        {
            timer_entry_t* next = t->next;
            free(t);
            t = next;
        }

    free(loop);
}

int cobalt_eventloop_add_fd(cobalt_eventloop_t* loop, int fd, short events, fd_handler_t cb,
                            void* user_data)
{
    if (!loop || fd < 0 || !cb)
        return -1;

#ifdef __linux__
    struct epoll_event ev = {0};
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
        {
            return -1;
        }
#elif __APPLE__
    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(loop->kq, &kev, 1, NULL, 0, NULL) < 0)
        {
            return -1;
        }
#endif

    fd_entry_t* entry = calloc(1, sizeof(fd_entry_t));
    if (!entry)
        return -1;
    entry->fd = fd;
    entry->events = events;
    entry->cb = cb;
    entry->user_data = user_data;

    if (!loop->fd_head)
        {
            loop->fd_head = loop->fd_tail = entry;
        }
    else
        {
            loop->fd_tail->next = entry;
            loop->fd_tail = entry;
        }

    return 0;
}

int cobalt_eventloop_mod_fd(cobalt_eventloop_t* loop, int fd, short events, fd_handler_t cb,
                            void* user_data)
{
    if (!loop || fd < 0)
        return -1;
    cobalt_eventloop_del_fd(loop, fd);
    return cobalt_eventloop_add_fd(loop, fd, events, cb, user_data);
}

int cobalt_eventloop_del_fd(cobalt_eventloop_t* loop, int fd)
{
    if (!loop || fd < 0)
        return -1;

#ifdef __linux__
    epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
#elif __APPLE__
    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(loop->kq, &kev, 1, NULL, 0, NULL);
#endif

    fd_entry_t** pp = &loop->fd_head;
    fd_entry_t* curr = *pp;
    while (curr)
        {
            if (curr->fd == fd)
                {
                    *pp = curr->next;
                    free(curr);
                    return 0;
                }
            pp = &curr->next;
            curr = curr->next;
        }
    return -1;
}

uint64_t cobalt_eventloop_add_timer(cobalt_eventloop_t* loop, uint64_t timeout_ms,
                                    uint64_t interval_ms, timer_handler_t cb, void* user_data)
{
    if (!loop || !cb)
        return 0;

    uint64_t id = ++loop->next_timer_id;
    timer_entry_t* entry = calloc(1, sizeof(timer_entry_t));
    if (!entry)
        return 0;

    entry->id = id;
    entry->timeout_ms = timeout_ms;
    entry->interval_ms = interval_ms;
    entry->cb = cb;
    entry->user_data = user_data;
    entry->active = 1;

    struct timespec now;
    get_time_now(&now);
    entry->next_fire.tv_sec = now.tv_sec + timeout_ms / 1000;
    entry->next_fire.tv_nsec = now.tv_nsec + (timeout_ms % 1000) * 1000000L;
    if (entry->next_fire.tv_nsec >= 1000000000L)
        {
            entry->next_fire.tv_sec++;
            entry->next_fire.tv_nsec -= 1000000000L;
        }

    if (!loop->timer_head)
        {
            loop->timer_head = entry;
        }
    else
        {
            /* Insert in sorted order by next_fire time */
            timer_entry_t *prev = NULL, *curr = loop->timer_head;
            while (curr && curr->next_fire.tv_sec < entry->next_fire.tv_sec)
                {
                    prev = curr;
                    curr = curr->next;
                }
            if (!prev)
                {
                    entry->next = loop->timer_head;
                    loop->timer_head = entry;
                }
            else
                {
                    prev->next = entry;
                    entry->next = curr;
                }
        }

    return id;
}

int cobalt_eventloop_del_timer(cobalt_eventloop_t* loop, uint64_t timer_id)
{
    if (!loop)
        return -1;

    timer_entry_t** pp = &loop->timer_head;
    timer_entry_t* curr = *pp;
    while (curr)
        {
            if (curr->id == timer_id)
                {
                    *pp = curr->next;
                    free(curr);
                    return 0;
                }
            pp = &curr->next;
            curr = curr->next;
        }
    return -1;
}

void cobalt_eventloop_run(cobalt_eventloop_t* loop)
{
    if (!loop)
        return;
    loop->running = 1;
    loop->stop_flag = 0;

    while (loop->running && !loop->stop_flag)
        {
            cobalt_eventloop_iteration(loop);
            usleep(1000);
        }
    loop->running = 0;
}

void cobalt_eventloop_stop(cobalt_eventloop_t* loop)
{
    if (loop)
        loop->stop_flag = 1;
}

int cobalt_eventloop_iteration(cobalt_eventloop_t* loop)
{
    if (!loop)
        return -1;

    struct timespec now;
    get_time_now(&now);

    /* Process expired timers */
    timer_entry_t* timer = loop->timer_head;
    while (timer && timer->active)
        {
            if (timer->next_fire.tv_sec < now.tv_sec ||
                (timer->next_fire.tv_sec == now.tv_sec && timer->next_fire.tv_nsec <= now.tv_nsec))
                {
                    if (timer->cb)
                        {
                            timer->cb(timer->id, timer->user_data);
                        }
                    if (timer->interval_ms > 0)
                        {
                            timer->next_fire.tv_sec += timer->interval_ms / 1000;
                            timer->next_fire.tv_nsec += (timer->interval_ms % 1000) * 1000000L;
                            if (timer->next_fire.tv_nsec >= 1000000000L)
                                {
                                    timer->next_fire.tv_sec++;
                                    timer->next_fire.tv_nsec -= 1000000000L;
                                }
                        }
                    else
                        {
                            timer->active = 0;
                        }
                }
            timer = timer->next;
        }

#ifdef __linux__
    /* Wait for FD events using epoll */
    /* Use a short timeout so tests don't hang when no timers/FDs are registered */
    int timeout_ms = 10;
    timer_entry_t* next_timer = loop->timer_head;
    if (next_timer)
        {
            long secs = next_timer->next_fire.tv_sec - now.tv_sec;
            long nsecs = next_timer->next_fire.tv_nsec - now.tv_nsec;
            timeout_ms = (int)(secs * 1000 + nsecs / 1000000L);
            if (timeout_ms < 0)
                timeout_ms = 0;
            if (timeout_ms > 100)
                timeout_ms = 100;
        }

    int n = epoll_wait(loop->epoll_fd, loop->epoll_events, loop->epoll_capacity, timeout_ms);
    for (int i = 0; i < n; i++)
        {
            fd_entry_t* entry = (fd_entry_t*)loop->epoll_events[i].data.ptr;
            if (entry && entry->cb)
                {
                    entry->cb(entry->fd, loop->epoll_events[i].events, entry->user_data);
                }
        }
#elif __APPLE__
    /* kqueue implementation */
    struct timespec ts = {0, 1000000}; /* 1ms timeout */
    struct kevent events[16];
    int n = kevent(loop->kq, NULL, 0, events, 16, &ts);
    for (int i = 0; i < n; i++)
        {
            int fd = (int)events[i].ident;
            fd_entry_t* entry = loop->fd_head;
            while (entry && entry->fd != fd)
                entry = entry->next;
            if (entry && entry->cb)
                {
                    short ev = (events[i].filter == EVFILT_READ) ? POLLIN : POLLOUT;
                    entry->cb(fd, ev, entry->user_data);
                }
        }
#endif

    return 0;
}
