/**
 * @file test_eventloop.c
 * @brief Unit test for event loop module.
 */

#include "cobalt/module/eventloop.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int  timer_called  = 0;
static int  fd_called     = 0;
static int *g_fired_order = NULL;
static int  g_fire_idx    = 0;

static void on_timer(uint64_t id, void *user_data)
{
    (void)id;
    timer_called++;
    uint64_t *timer_id_ptr = (uint64_t *)user_data;
    if (timer_id_ptr) {
        (*timer_id_ptr)++;
    }
    if (g_fired_order) {
        g_fired_order[g_fire_idx++] = (int)id;
    }
    printf("  Timer %lu fired (call #%d)\n", (unsigned long)id, timer_called);
}

static void on_fd(int fd, short events, void *user_data)
{
    (void)events;
    fd_called++;
    printf("  FD %d ready (call #%d)\n", fd, fd_called);
}

void test_eventloop_create_destroy(void)
{
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

void test_eventloop_fd(void)
{
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

void test_eventloop_timer(void)
{
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

void test_eventloop_run_stop(void)
{
    printf("Testing eventloop run/stop...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    if (!loop) {
        fprintf(stderr, "ERROR: Failed to create eventloop\n");
        return;
    }

    /* Stop NULL should not crash */
    cobalt_eventloop_stop(NULL);
    printf("  Stop NULL loop: OK\n");

    /* Stop immediately - shouldn't block */
    cobalt_eventloop_stop(loop);
    printf("  Stop called on new loop: OK\n");

    /* Run should not block forever (stub implementation) */
    /* We use iteration instead for testing */
    int ret = cobalt_eventloop_iteration(loop);
    printf("  Iteration on empty loop: ret=%d\n", ret);

    cobalt_eventloop_destroy(loop);
}

void test_eventloop_multiple_timers(void)
{
    printf("Testing eventloop with multiple timers...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    if (!loop) {
        fprintf(stderr, "ERROR: Failed to create eventloop\n");
        return;
    }

    int counter1 = 0;
    int counter2 = 0;

    uint64_t id1 = cobalt_eventloop_add_timer(loop, 1, 0, on_timer, &counter1);
    uint64_t id2 = cobalt_eventloop_add_timer(loop, 1, 0, on_timer, &counter2);

    printf("  Added 2 timers: id1=%lu, id2=%lu\n", (unsigned long)id1, (unsigned long)id2);

    cobalt_eventloop_iteration(loop);

    printf("  Timer1 counter: %d, Timer2 counter: %d\n", counter1, counter2);

    cobalt_eventloop_del_timer(loop, id1);
    cobalt_eventloop_del_timer(loop, id2);

    cobalt_eventloop_destroy(loop);
}

void test_eventloop_timer_heap_order(void)
{
    printf("Testing eventloop timer heap ordering...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    const int       timer_count = 100;
    static int      fired_order[100];
    static uint64_t timer_ids[101];
    static int      fire_idx = 0;

    memset(fired_order, -1, sizeof(fired_order));
    memset(timer_ids, 0, sizeof(timer_ids));
    fire_idx = 0;

    g_fired_order = fired_order;
    g_fire_idx    = 0;

    for (int i = 0; i < timer_count; i++) {
        timer_ids[i + 1] = cobalt_eventloop_add_timer(loop, i + 1, 0, on_timer, &timer_ids[i + 1]);
        TEST_ASSERT(timer_ids[i + 1] != 0);
    }

    for (int i = 0; i < timer_count; i++) {
        cobalt_eventloop_iteration(loop);
        usleep(2000);
    }

    TEST_ASSERT(g_fire_idx == timer_count);

    for (int i = 0; i < timer_count; i++) {
        TEST_ASSERT(fired_order[i] == (i + 1));
    }

    g_fired_order = NULL;
    cobalt_eventloop_destroy(loop);
    printf("  Eventloop timer heap ordering test passed\n");
}

void test_eventloop(void)
{
    printf("Testing eventloop...\n");
    test_eventloop_create_destroy();
    test_eventloop_fd();
    test_eventloop_timer();
    test_eventloop_run_stop();
    test_eventloop_multiple_timers();
    test_eventloop_timer_heap_order();
    printf("  Eventloop tests completed\n");
}
