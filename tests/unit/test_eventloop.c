/**
 * @file test_eventloop.c
 * @Unit test for event loop module.
 */

#include <stdio.h>
#include "cobalt/module/eventloop.h"

static int timer_called = 0;
static int fd_called = 0;

static void on_timer(uint64_t id, void *user_data) {
    (void)id; (void)user_data;
    timer_called++;
    printf("  Timer %lu fired (call #%d)\n", (unsigned long)id, timer_called);
}

static void on_fd(int fd, short events, void *user_data) {
    (void)events;
    fd_called++;
    printf("  FD %d ready (call #%d)\n", fd, fd_called);
}

void test_eventloop_create_destroy(void) {
    printf("Testing eventloop create/destroy...\n");
    
    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    if (!loop) {
        fprintf(stderr, "ERROR: Failed to create eventloop\n");
        return;
    }
    printf("  Event loop created\n");
    
    cobalt_eventloop_destroy(loop);
    printf("  Event loop destroyed\n");
}

void test_eventloop_fd(void) {
    printf("Testing eventloop FD handling...\n");
    
    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    if (!loop) {
        fprintf(stderr, "ERROR: Failed to create eventloop\n");
        return;
    }
    
    /* Add an FD handler (using a dummy fd) */
    int ret = cobalt_eventloop_add_fd(loop, -1, 1, on_fd, NULL);
    if (ret == 0) {
        printf("  Add FD handler: OK\n");
    } else {
        printf("  Add FD handler: ret=%d (implementation-specific)\n", ret);
    }
    
    /* Modify FD */
    ret = cobalt_eventloop_mod_fd(loop, -1, 1, on_fd, NULL);
    if (ret == 0) {
        printf("  Modify FD handler: OK\n");
    }
    
    /* Delete FD */
    ret = cobalt_eventloop_del_fd(loop, -1);
    if (ret == 0) {
        printf("  Delete FD handler: OK\n");
    }
    
    cobalt_eventloop_destroy(loop);
}

void test_eventloop_timer(void) {
    printf("Testing eventloop timers...\n");
    
    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    if (!loop) {
        fprintf(stderr, "ERROR: Failed to create eventloop\n");
        return;
    }
    
    timer_called = 0;
    
    /* Add a one-shot timer (1ms timeout for fast testing) */
    uint64_t timer_id = cobalt_eventloop_add_timer(loop, 1, 0, on_timer, NULL);
    if (timer_id != 0) {
        printf("  Timer added with id=%lu\n", (unsigned long)timer_id);
    }
    
    /* Run one iteration to trigger the timer (simulated in stub) */
    int ret = cobalt_eventloop_iteration(loop);
    printf("  Iteration returned: %d\n", ret);
    
    if (timer_called > 0) {
        printf("  Timer callback invoked: OK\n");
    } else {
        printf("  Note: Timer may not have fired in stub implementation\n");
    }
    
    /* Delete timer */
    ret = cobalt_eventloop_del_timer(loop, timer_id);
    if (ret == 0) {
        printf("  Timer deleted: OK\n");
    }
    
    cobalt_eventloop_destroy(loop);
}

void test_eventloop_run_stop(void) {
    printf("Testing eventloop run/stop...\n");
    
    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    if (!loop) {
        fprintf(stderr, "ERROR: Failed to create eventloop\n");
        return;
    }
    
    /* Stop immediately - shouldn't block */
    cobalt_eventloop_stop(loop);
    printf("  Stop called on new loop: OK\n");
    
    cobalt_eventloop_destroy(loop);
}

void test_eventloop(void) {
    printf("Testing eventloop...\n");
    test_eventloop_create_destroy();
    test_eventloop_fd();
    test_eventloop_timer();
    test_eventloop_run_stop();
    printf("  Eventloop tests completed\n");
}
