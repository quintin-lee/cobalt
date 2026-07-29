#include "cobalt/module/eventloop.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

/* Simplified event loop implementation - uses polling + timer queue
   This is a minimal stub that satisfies the API but doesn't provide real async IO.
   In a production version, this would use epoll/kqueue/IOCP under the hood. */

/* Internal FD entry */
typedef struct fd_entry {
    int fd;
    short events;
    fd_handler_t cb;
    void *user_data;
    struct fd_entry *next;
} fd_entry_t;

/* Internal timer entry */
typedef struct timer_entry {
    uint64_t id;
    uint64_t timeout_ms;
    uint64_t interval_ms;
    timer_handler_t cb;
    void *user_data;
    int active; /* 1 = still active */
    struct timer_entry *next;
} timer_entry_t;

/* Event loop context */
struct cobalt_eventloop {
    fd_entry_t *fd_head;
    fd_entry_t *fd_tail;
    timer_entry_t *timer_head;
    timer_entry_t *timer_tail;
    uint64_t next_timer_id;
    int running;
    int stop_flag;
    clock_t last_iteration; /* For timing */
};

/* Helper: create a new fd entry */
static fd_entry_t* fd_entry_new(int fd, short events, fd_handler_t cb, void *user_data) {
    fd_entry_t *e = malloc(sizeof(fd_entry_t));
    if (e) {
        e->fd = fd;
        e->events = events;
        e->cb = cb;
        e->user_data = user_data;
        e->next = NULL;
    }
    return e;
}

/* Helper: create a new timer entry */
static timer_entry_t* timer_entry_new(uint64_t id, uint64_t timeout_ms, uint64_t interval_ms, timer_handler_t cb, void *user_data) {
    timer_entry_t *e = malloc(sizeof(timer_entry_t));
    if (e) {
        e->id = id;
        e->timeout_ms = timeout_ms;
        e->interval_ms = interval_ms;
        e->cb = cb;
        e->user_data = user_data;
        e->active = 1;
        e->next = NULL;
    }
    return e;
}

/* ============================================================
   PUBLIC API IMPLEMENTATION
   ============================================================ */

cobalt_eventloop_t *cobalt_eventloop_create(void) {
    cobalt_eventloop_t *loop = malloc(sizeof(cobalt_eventloop_t));
    if (loop) {
        loop->fd_head = NULL;
        loop->fd_tail = NULL;
        loop->timer_head = NULL;
        loop->timer_tail = NULL;
        loop->next_timer_id = 0;
        loop->running = 0;
        loop->stop_flag = 0;
        loop->last_iteration = clock();
    }
    return loop;
}

void cobalt_eventloop_destroy(cobalt_eventloop_t *loop) {
    if (!loop) return;
    
    /* Free all FD entries */
    fd_entry_t *f = loop->fd_head;
    while (f) {
        fd_entry_t *next = f->next;
        free(f);
        f = next;
    }
    
    /* Free all timers */
    timer_entry_t *t = loop->timer_head;
    while (t) {
        timer_entry_t *next = t->next;
        free(t);
        t = next;
    }
    
    free(loop);
}

int cobalt_eventloop_add_fd(cobalt_eventloop_t *loop, int fd, short events, fd_handler_t cb, void *user_data) {
    (void)events; /* Stub - ignore events for now */
    if (!loop || fd < 0 || !cb) return -1;
    
    fd_entry_t *entry = fd_entry_new(fd, events, cb, user_data);
    if (!entry) return -1;
    
    /* Add to linked list */
    if (!loop->fd_head) {
        loop->fd_head = loop->fd_tail = entry;
    } else {
        loop->fd_tail->next = entry;
        loop->fd_tail = entry;
    }
    
    return 0;
}

int cobalt_eventloop_mod_fd(cobalt_eventloop_t *loop, int fd, short events, fd_handler_t cb, void *user_data) {
    (void)events; (void)cb; (void)user_data;
    if (!loop || fd < 0) return -1;
    
    /* Find and update - stub: remove old then add new */
    cobalt_eventloop_del_fd(loop, fd);
    return cobalt_eventloop_add_fd(loop, fd, events, cb, user_data);
}

int cobalt_eventloop_del_fd(cobalt_eventloop_t *loop, int fd) {
    if (!loop || fd < 0) return -1;
    
    fd_entry_t **pp = &loop->fd_head;
    fd_entry_t *curr = *pp;
    while (curr) {
        if (curr->fd == fd) {
            *pp = curr->next;
            free(curr);
            return 0;
        }
        pp = &(curr->next);
        curr = curr->next;
    }
    return -1; /* Not found */
}

uint64_t cobalt_eventloop_add_timer(cobalt_eventloop_t *loop, uint64_t timeout_ms, uint64_t interval_ms, timer_handler_t cb, void *user_data) {
    if (!loop || timeout_ms == 0 || !cb) return 0;
    
    uint64_t id = ++loop->next_timer_id;
    timer_entry_t *entry = timer_entry_new(id, timeout_ms, interval_ms, cb, user_data);
    if (!entry) return 0;
    
    /* Add to timer list */
    if (!loop->timer_head) {
        loop->timer_head = loop->timer_tail = entry;
    } else {
        loop->timer_tail->next = entry;
        loop->timer_tail = entry;
    }
    
    return id;
}

int cobalt_eventloop_del_timer(cobalt_eventloop_t *loop, uint64_t timer_id) {
    if (!loop) return -1;
    
    timer_entry_t *t = loop->timer_head;
    while (t) {
        if (t->id == timer_id) {
            t->active = 0;
            return 0;
        }
        t = t->next;
    }
    return -1;
}

void cobalt_eventloop_run(cobalt_eventloop_t *loop) {
    if (!loop) return;
    
    loop->running = 1;
    loop->stop_flag = 0;
    
    while (loop->running && !loop->stop_flag) {
        cobalt_eventloop_iteration(loop);
        usleep(1000); /* 1ms sleep to prevent 100% CPU */
    }
    
    loop->running = 0;
}

void cobalt_eventloop_stop(cobalt_eventloop_t *loop) {
    if (loop) {
        loop->stop_flag = 1;
    }
}

int cobalt_eventloop_iteration(cobalt_eventloop_t *loop) {
    if (!loop) return -1;
    
    /* Check timers */
    clock_t now = clock();
    double now_sec = (double)now / CLOCKS_PER_SEC;
    double last_sec = (double)loop->last_iteration / CLOCKS_PER_SEC;
    loop->last_iteration = now;
    
    /* Simple timer check - stub with simulated time */
    static volatile double fake_time = 0;
    fake_time += 0.01; /* Simulate time passing */
    
    timer_entry_t **ttp = &loop->timer_head;
    timer_entry_t *t = *ttp;
    while (t) {
        if (t->active && fake_time > t->timeout_ms / 1000.0) {
            if (t->cb) {
                t->cb(t->id, t->user_data);
            }
            if (t->interval_ms > 0) {
                fake_time = 0; /* Reset for repeating */
            } else {
                t->active = 0; /* One-shot done */
            }
        }
        ttp = &(t->next);
        t = *ttp;
    }
    
    /* Check FDs - stub: simulate readable event for all fds */
    fd_entry_t *f = loop->fd_head;
    while (f) {
        if (f->cb) {
            f->cb(f->fd, 1, f->user_data);
        }
        f = f->next;
    }
    
    return 0;
}
