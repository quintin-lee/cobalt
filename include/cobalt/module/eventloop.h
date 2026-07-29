#ifndef EVENTLOOP_H
#define EVENTLOOP_H

/**
 * @file eventloop.h
 * @brief Event-driven I/O loop module
 */

#include <stddef.h>
#include <stdint.h>

/* File descriptor handler callback */
typedef void (*fd_handler_t)(int fd, short events, void *user_data);

/* Timer callback */
typedef void (*timer_handler_t)(uint64_t id, void *user_data);

/* Event loop structure */
typedef struct cobalt_eventloop cobalt_eventloop_t;

/* Create a new event loop */
cobalt_eventloop_t *cobalt_eventloop_create(void);

/* Destroy event loop */
void cobalt_eventloop_destroy(cobalt_eventloop_t *loop);

/* Register file descriptor handler */
int cobalt_eventloop_add_fd(cobalt_eventloop_t *loop, int fd, short events, fd_handler_t cb, void *user_data);

/* Modify fd events */
int cobalt_eventloop_mod_fd(cobalt_eventloop_t *loop, int fd, short events, fd_handler_t cb, void *user_data);

/* Remove fd handler */
int cobalt_eventloop_del_fd(cobalt_eventloop_t *loop, int fd);

/* Add timer */
uint64_t cobalt_eventloop_add_timer(cobalt_eventloop_t *loop, uint64_t timeout_ms, uint64_t interval_ms, timer_handler_t cb, void *user_data);

/* Remove timer */
int cobalt_eventloop_del_timer(cobalt_eventloop_t *loop, uint64_t timer_id);

/* Run the event loop (blocking) */
void cobalt_eventloop_run(cobalt_eventloop_t *loop);

/* Stop the event loop */
void cobalt_eventloop_stop(cobalt_eventloop_t *loop);

/* Process one iteration (non-blocking) */
int cobalt_eventloop_iteration(cobalt_eventloop_t *loop);

#endif /* EVENTLOOP_H */
