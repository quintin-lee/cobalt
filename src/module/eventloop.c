/**
 * @file eventloop.c
 * @brief Implementation of the event-driven I/O loop module
 */
#define _POSIX_C_SOURCE 200112L
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
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

#define COBALT_EVENTLOOP_DEFAULT_CAPACITY 16
#define COBALT_EVENTLOOP_TIMEOUT_MS 10
#define COBALT_EVENTLOOP_TIMEOUT_MAX 100
#define COBALT_EVENTLOOP_TIMER_HEAP_INITIAL 16
#define COBALT_MILLIS_PER_SEC 1000ULL
#define COBALT_NANOS_PER_MILLI 1000000ULL
#define COBALT_NANOS_PER_SEC 1000000000LL

/* ============================================================
 * Internal data structures
 * ============================================================ */

/**
 * @brief Internal timer entry structure
 * Records timer trigger time, callback, and status information.
 */
typedef struct timer_entry {
    uint64_t timer_id;    /**< Unique timer ID */
    uint64_t timeout_ms;  /**< Delay time for the first trigger (milliseconds) */
    uint64_t interval_ms; /**< Interval time for repeating timer (milliseconds), 0 means one-shot */
    timer_handler_t     callback;  /**< Timer callback function */
    void               *user_data; /**< User data for the callback function */
    int                 active;    /**< Whether the timer is currently active */
    struct timespec     next_fire; /**< Absolute time for the next trigger */
    struct timer_entry *next;      /**< Next pointer for linked list structure (currently mainly
                                      maintained by array heap) */
} timer_entry_t;

/**
 * @brief Internal file descriptor event entry structure
 * Saves listened FDs and their callbacks in a linked list form.
 */
typedef struct fd_entry {
    int              fd;        /**< The file descriptor being listened to */
    short            events;    /**< Mask of events to listen for */
    fd_handler_t     callback;  /**< Callback function when event occurs */
    void            *user_data; /**< User data for the callback function */
    struct fd_entry *next;      /**< The next entry in the linked list */
} fd_entry_t;

/* ============================================================
 * Min-heap helper functions (used for managing timers)
 * ============================================================ */

/* Compare trigger times of two timer entries. Returns negative if a < b */
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
    return 0;
}

/* Sift up operation in the timer heap to maintain min-heap property */
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

/* Sift down operation in the timer heap to maintain min-heap property */
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
 * @brief Specific definition of the event loop context
 */
struct cobalt_eventloop {
#ifdef __linux__
    int                 epoll_fd;       /**< epoll file descriptor */
    struct epoll_event *epoll_events;   /**< Array to store epoll_wait results */
    int                 epoll_capacity; /**< Capacity of the epoll events array */
#elif __APPLE__
    int kq; /**< kqueue file descriptor */
#endif
    fd_entry_t     *fd_head;        /**< Head pointer of the FD listening linked list */
    fd_entry_t     *fd_tail;        /**< Tail pointer of the FD listening linked list */
    timer_entry_t **timer_heap;     /**< Min-heap array managing timers */
    int             timer_count;    /**< Current number of timers in the heap */
    int             timer_capacity; /**< Capacity of the heap */
    uint64_t        next_timer_id;  /**< The next allocated timer ID */
    int             running;        /**< Flag indicating if the event loop is running */
    int             stop_flag;      /**< Flag requesting to stop the event loop */
};

/* Heap operation helper functions bound to the event loop object */

/* Push a timer into the heap */
static void heap_push(cobalt_eventloop_t *loop, timer_entry_t *entry)
{
    // If capacity is insufficient, expand it
    if (loop->timer_count >= loop->timer_capacity) {
        int             new_cap  = loop->timer_capacity * 2;
        timer_entry_t **new_heap = realloc(loop->timer_heap, sizeof(timer_entry_t *) * new_cap);
        if (!new_heap) {
            return;
        }
        loop->timer_heap     = new_heap;
        loop->timer_capacity = new_cap;
    }
    loop->timer_heap[loop->timer_count] = entry;
    heap_sift_up(loop->timer_heap, loop->timer_count);
    loop->timer_count++;
}

/* Pop and return the timer with the nearest trigger time in the heap */
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

/* Peek at the timer with the nearest trigger time in the heap, without popping */
static timer_entry_t *heap_peek(const cobalt_eventloop_t *loop)
{
    if (loop->timer_count == 0) {
        return NULL;
    }
    return loop->timer_heap[0];
}

/* ============================================================
   Helper functions
   ============================================================ */

/* Get monotonic time (unaffected by system clock adjustments, suitable for timers) */
static void get_time_now(struct timespec *timepoint)
{
    clock_gettime(CLOCK_MONOTONIC, timepoint);
}

/* Check if the time represented by lhs is later than or equal to the time represented by rhs (now)
 */
static int timer_expired(const struct timespec *lhs, const struct timespec *rhs)
{
    long secs  = lhs->tv_sec - rhs->tv_sec;
    long nsecs = lhs->tv_nsec - rhs->tv_nsec;
    return (secs > 0) || (secs == 0 && nsecs >= 0);
}

/* ============================================================
   Private logic handling
   ============================================================ */

/* Check and process all expired timers */
static void process_expired_timers(cobalt_eventloop_t *loop, const struct timespec *now)
{
    while (loop->timer_count > 0) {
        timer_entry_t *timer = heap_peek(loop);
        if (!timer || !timer->active) {
            break;
        }
        if (!timer_expired(&timer->next_fire, now)) {
            break;
        }

        heap_pop_min(loop);

        if (timer->callback != NULL) {
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
            free(timer);
        }
    }
}

/* Calculate the timeout to wait for epoll_wait/kevent */
static int calculate_timeout_ms(const cobalt_eventloop_t *loop, const struct timespec *now)
{
    int                  timeout_ms = COBALT_EVENTLOOP_TIMEOUT_MS;
    const timer_entry_t *next_timer = heap_peek(loop);
    if (next_timer != NULL) {
        long secs  = next_timer->next_fire.tv_sec - now->tv_sec;
        long nsecs = next_timer->next_fire.tv_nsec - now->tv_nsec;
        // Calculate the remaining milliseconds until the next timer triggers
        timeout_ms = (int)((secs * COBALT_MILLIS_PER_SEC) + (nsecs / COBALT_NANOS_PER_MILLI));
        if (timeout_ms < 0) {
            timeout_ms = 0;
        }
        // Set a maximum value to prevent waiting too long
        if (timeout_ms > COBALT_EVENTLOOP_TIMEOUT_MAX) {
            timeout_ms = COBALT_EVENTLOOP_TIMEOUT_MAX;
        }
    }
    return timeout_ms;
}

/* ============================================================
   Public interface implementation
   ============================================================ */

/*
 * @brief Create event loop object
 * Initializes the internal structures and the underlying IO multiplexing mechanism (epoll/kqueue).
 */
cobalt_eventloop_t *cobalt_eventloop_create(void)
{
    cobalt_eventloop_t *loop = calloc(1, sizeof(cobalt_eventloop_t));
    if (loop == NULL) {
        return NULL;
    }

#ifdef __linux__
    loop->epoll_fd = epoll_create1(0);
    if (loop->epoll_fd < 0) {
        free(loop);
        return NULL;
    }
    loop->epoll_capacity = COBALT_EVENTLOOP_DEFAULT_CAPACITY;
    loop->epoll_events   = malloc(sizeof(struct epoll_event) * loop->epoll_capacity);
    if (loop->epoll_events == NULL) {
        close(loop->epoll_fd);
        free(loop);
        return NULL;
    }
#elif __APPLE__
    loop->kq = kqueue();
    if (loop->kq < 0) {
        free(loop);
        return NULL;
    }
#endif

    loop->timer_capacity = COBALT_EVENTLOOP_TIMER_HEAP_INITIAL;
    loop->timer_heap     = malloc(sizeof(timer_entry_t *) * loop->timer_capacity);
    if (loop->timer_heap == NULL) {
#ifdef __linux__
        close(loop->epoll_fd);
        free(loop->epoll_events);
#elif __APPLE__
        close(loop->kq);
#endif
        free(loop);
        return NULL;
    }

    return loop;
}

/*
 * @brief Destroy the event loop
 * Closes the underlying file descriptors, frees all entries in the FD linked list and timer heap,
 * as well as the loop's own memory.
 */
void cobalt_eventloop_destroy(cobalt_eventloop_t *loop)
{
    if (loop == NULL) {
        return;
    }

#ifdef __linux__
    if (loop->epoll_fd >= 0) {
        close(loop->epoll_fd);
    }
    free(loop->epoll_events);
#elif __APPLE__
    if (loop->kq >= 0) {
        close(loop->kq);
    }
#endif

    fd_entry_t *current_fd = loop->fd_head;
    while (current_fd != NULL) {
        fd_entry_t *next = current_fd->next;
        free(current_fd);
        current_fd = next;
    }

    for (int i = 0; i < loop->timer_count; i++) {
        free(loop->timer_heap[i]);
    }
    free(loop->timer_heap);

    free(loop);
}

/*
 * @brief Add a file descriptor to the event loop's listeners
 * Wraps and calls the underlying epoll_ctl or kevent, and saves the related state to the internal
 * linked list.
 */
int cobalt_eventloop_add_fd(
    cobalt_eventloop_t *loop,
    cobalt_fd_t         file_descriptor, // NOLINT(bugprone-easily-swappable-parameters)
    cobalt_events_t     events,
    fd_handler_t        callback,
    void               *user_data)
{
    if (loop == NULL || file_descriptor < 0 || callback == NULL) {
        return -1;
    }

#ifdef __linux__
    struct epoll_event ev_data = {0};
    ev_data.events             = events;
    ev_data.data.fd            = file_descriptor;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, file_descriptor, &ev_data) < 0) {
        return -1;
    }
#elif __APPLE__
    struct kevent kev;
    EV_SET(&kev, file_descriptor, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(loop->kq, &kev, 1, NULL, 0, NULL) < 0) {
        return -1;
    }
#endif

    fd_entry_t *entry = calloc(1, sizeof(fd_entry_t));
    if (entry == NULL) {
        return -1;
    }
    entry->fd        = file_descriptor;
    entry->events    = events;
    entry->callback  = callback;
    entry->user_data = user_data;

    if (loop->fd_head == NULL) {
        loop->fd_head = loop->fd_tail = entry;
    } else {
        loop->fd_tail->next = entry;
        loop->fd_tail       = entry;
    }

    return 0;
}

/*
 * @brief Modify the listened events of an already listening file descriptor
 * This is done by deleting first and then re-adding.
 */
int cobalt_eventloop_mod_fd(cobalt_eventloop_t *loop,
                            cobalt_fd_t         file_descriptor,
                            cobalt_events_t     events,
                            fd_handler_t        callback,
                            void               *user_data)
{
    if (loop == NULL || file_descriptor < 0) {
        return -1;
    }
    cobalt_eventloop_del_fd(loop, file_descriptor);
    return cobalt_eventloop_add_fd(loop, file_descriptor, events, callback, user_data);
}

/*
 * @brief Remove a file descriptor from the event loop's listeners
 * Removes it from both the underlying epoll/kqueue and the internal listening linked list
 * simultaneously.
 */
int cobalt_eventloop_del_fd(cobalt_eventloop_t *loop, cobalt_fd_t file_descriptor)
{
    if (loop == NULL || file_descriptor < 0) {
        return -1;
    }

#ifdef __linux__
    epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, file_descriptor, NULL);
#elif __APPLE__
    struct kevent kev;
    EV_SET(&kev, file_descriptor, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(loop->kq, &kev, 1, NULL, 0, NULL);
#endif

    fd_entry_t **prev_ptr = &loop->fd_head;
    fd_entry_t  *current  = *prev_ptr;
    while (current != NULL) {
        if (current->fd == file_descriptor) {
            *prev_ptr = current->next;
            free(current);
            return 0;
        }
        prev_ptr = &current->next;
        current  = current->next;
    }
    return -1;
}

/*
 * @brief Add a new timer task
 * Creates a new timer structure based on the given parameters, calculates its absolute trigger
 * time, and puts it in the min-heap for management.
 */
uint64_t cobalt_eventloop_add_timer(
    cobalt_eventloop_t  *loop,
    cobalt_timeout_ms_t  timeout_ms, // NOLINT(bugprone-easily-swappable-parameters)
    cobalt_interval_ms_t interval_ms,
    timer_handler_t      callback,
    void                *user_data)
{
    if (loop == NULL || callback == NULL) {
        return 0;
    }

    uint64_t       timer_id = ++loop->next_timer_id;
    timer_entry_t *entry    = calloc(1, sizeof(timer_entry_t));
    if (entry == NULL) {
        return 0;
    }

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

    heap_push(loop, entry);

    return timer_id;
}

/*
 * @brief Delete the specified timer
 * Finds the matching ID in the heap, removes it, and readapts the heap to maintain the min-heap
 * property.
 */
int cobalt_eventloop_del_timer(cobalt_eventloop_t *loop, uint64_t timer_id)
{
    if (loop == NULL) {
        return -1;
    }

    for (int i = 0; i < loop->timer_count; i++) {
        if (loop->timer_heap[i]->timer_id == timer_id) {
            loop->timer_count--;
            loop->timer_heap[i] = loop->timer_heap[loop->timer_count];
            heap_sift_down(loop->timer_heap, loop->timer_count, i);
            free(loop->timer_heap[loop->timer_count]);
            return 0;
        }
    }
    return -1;
}

/*
 * @brief Run the event loop until the stop flag is received
 * In the internal loop, it continuously calls cobalt_eventloop_iteration to process expired tasks
 * and IO events.
 */
void cobalt_eventloop_run(cobalt_eventloop_t *loop)
{
    if (loop == NULL) {
        return;
    }
    loop->running   = 1;
    loop->stop_flag = 0;

    while (loop->running && !loop->stop_flag) {
        cobalt_eventloop_iteration(loop);
        usleep(COBALT_MILLIS_PER_SEC);
    }
    loop->running = 0;
}

/*
 * @brief Stop the event loop
 * Sets the stop_flag so that the next loop condition check fails.
 */
void cobalt_eventloop_stop(cobalt_eventloop_t *loop)
{
    if (loop != NULL) {
        loop->stop_flag = 1;
    }
}

/*
 * @brief Execute a single event handling iteration
 * Mainly executes two steps:
 * 1. Process all expired timer callbacks.
 * 2. Block waiting (timeout determined by the nearest timer) and handle occurred I/O events.
 */
int cobalt_eventloop_iteration(cobalt_eventloop_t *loop)
{
    if (loop == NULL) {
        return -1;
    }

    struct timespec now;
    get_time_now(&now);

    /* Process expired timers */
    process_expired_timers(loop, &now);

#ifdef __linux__
    /* Wait for FD events using epoll */
    /* Use a short timeout so tests don't hang when no timers/FDs are registered */
    int timeout_ms = calculate_timeout_ms(loop, &now);
    int event_count =
        epoll_wait(loop->epoll_fd, loop->epoll_events, loop->epoll_capacity, timeout_ms);
    for (int i = 0; i < event_count; i++) {
        fd_entry_t *entry = (fd_entry_t *)loop->epoll_events[i].data.ptr;
        if (entry != NULL && entry->callback != NULL) {
            entry->callback(entry->fd, (short)loop->epoll_events[i].events, entry->user_data);
        }
    }
#elif __APPLE__
    /* kqueue implementation */
    struct timespec ts = {0, COBALT_NANOS_PER_MILLI}; /* 1ms timeout */
    struct kevent   events[COBALT_EVENTLOOP_DEFAULT_CAPACITY];
    int event_count = kevent(loop->kq, NULL, 0, events, COBALT_EVENTLOOP_DEFAULT_CAPACITY, &ts);
    for (int i = 0; i < event_count; i++) {
        int         fd    = (int)events[i].ident;
        fd_entry_t *entry = loop->fd_head;
        while (entry != NULL && entry->fd != fd) {
            entry = entry->next;
        }
        if (entry != NULL && entry->callback != NULL) {
            short ev = (events[i].filter == EVFILT_READ) ? POLLIN : POLLOUT;
            entry->callback(fd, ev, entry->user_data);
        }
    }
#endif

    return 0;
}
