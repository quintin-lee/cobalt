#include "module/eventloop.h"
#include <stdlib.h>

cobalt_eventloop_t *cobalt_eventloop_create(void) {
  return NULL; /* Simplified */
}

void cobalt_eventloop_destroy(cobalt_eventloop_t *loop) {
  (void)loop;
}

int cobalt_eventloop_add_fd(cobalt_eventloop_t *loop, int fd, short events, fd_handler_t cb, void *user_data) {
  (void)loop; (void)fd; (void)events; (void)cb; (void)user_data;
  return 0;
}

int cobalt_eventloop_mod_fd(cobalt_eventloop_t *loop, int fd, short events, fd_handler_t cb, void *user_data) {
  (void)loop; (void)fd; (void)events; (void)cb; (void)user_data;
  return 0;
}

int cobalt_eventloop_del_fd(cobalt_eventloop_t *loop, int fd) {
  (void)loop; (void)fd;
  return 0;
}

uint64_t cobalt_eventloop_add_timer(cobalt_eventloop_t *loop, uint64_t timeout_ms, uint64_t interval_ms, timer_handler_t cb, void *user_data) {
  (void)loop; (void)timeout_ms; (void)interval_ms; (void)cb; (void)user_data;
  return 0;
}

int cobalt_eventloop_del_timer(cobalt_eventloop_t *loop, uint64_t timer_id) {
  (void)loop; (void)timer_id;
  return 0;
}

void cobalt_eventloop_run(cobalt_eventloop_t *loop) {
  (void)loop;
}

void cobalt_eventloop_stop(cobalt_eventloop_t *loop) {
  (void)loop;
}

int cobalt_eventloop_iteration(cobalt_eventloop_t *loop) {
  (void)loop;
  return 0;
}
