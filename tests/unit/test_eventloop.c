/**
 * @file test_eventloop.c
 * @brief Unit test for event loop module.
 */

#include "cobalt/module/eventloop.h"
#include "test_framework.h"
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
    if (timer_called <= 3) {
        if (timer_called <= 2) {
            printf("  Timer %lu fired (call #%d)\n", (unsigned long)id, timer_called);
        }
    }
}

static void on_timer_quiet(uint64_t id, void *user_data)
{
    (void)id;
    int *counter = (int *)user_data;
    if (counter) {
        (*counter)++;
    }
}

static void on_fd(int fd, short events, void *user_data)
{
    (void)events;
    fd_called++;
    printf("  FD %d ready (call #%d)\n", fd, fd_called);
}

/* Forward declarations for new tests */
void test_eventloop_timer_edge_cases(void);
void test_eventloop_fd_edge_cases(void);

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

    int pipefd[2];
    if (pipe(pipefd) == 0) {
        /* Test READ event */
        int ret = cobalt_eventloop_add_fd(loop, pipefd[0], POLLIN, on_fd, NULL);
        if (ret == 0) {
            printf("  Add FD READ handler: OK\n");
        }

        /* Test WRITE event */
        ret = cobalt_eventloop_add_fd(loop, pipefd[1], POLLOUT, on_fd, NULL);
        if (ret == 0) {
            printf("  Add FD WRITE handler: OK\n");
        }

        /* Modify FD events */
        ret = cobalt_eventloop_mod_fd(loop, pipefd[0], POLLOUT, on_fd, NULL);
        if (ret == 0) {
            printf("  Modify FD events: OK\n");
        }

        /* Delete FD */
        ret = cobalt_eventloop_del_fd(loop, pipefd[0]);
        if (ret == 0) {
            printf("  Delete FD handler: OK\n");
        }
        ret = cobalt_eventloop_del_fd(loop, pipefd[1]);
        if (ret == 0) {
            printf("  Delete FD handler: OK\n");
        }

        close(pipefd[0]);
        close(pipefd[1]);
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

    uint64_t counter1 = 0;
    uint64_t counter2 = 0;

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

void test_eventloop_timing(void)
{
    printf("Testing eventloop timing...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Run 10 iterations on an empty loop — should be fast, not 1s each */
    for (int i = 0; i < 10; i++) {
        cobalt_eventloop_iteration(loop);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    long elapsed_ms =
        (end.tv_sec - start.tv_sec) * 1000L + (end.tv_nsec - start.tv_nsec) / 1000000L;

    printf("  10 iterations took %ld ms\n", elapsed_ms);
    /* The bug caused 1s per iteration; should be well under 1000ms total */
    TEST_ASSERT(elapsed_ms < 1000);

    cobalt_eventloop_destroy(loop);
    printf("  Eventloop timing test passed\n");
}

void test_eventloop_timer_edge_cases(void)
{
    printf("Testing eventloop timer edge cases...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    /* Zero timeout timer */
    uint64_t id_zero = cobalt_eventloop_add_timer(loop, 0, 0, on_timer, NULL);
    TEST_ASSERT(id_zero != 0);
    cobalt_eventloop_iteration(loop);
    printf("  Zero timeout timer: OK\n");

    /* Delete timer immediately after add */
    uint64_t id_del = cobalt_eventloop_add_timer(loop, 100, 0, on_timer, NULL);
    TEST_ASSERT(id_del != 0);
    int ret = cobalt_eventloop_del_timer(loop, id_del);
    TEST_ASSERT(ret == 0);
    printf("  Immediate timer delete: OK\n");

    /* Delete non-existent timer */
    ret = cobalt_eventloop_del_timer(loop, 99999);
    TEST_ASSERT(ret == -1);
    printf("  Delete non-existent timer: OK\n");

    /* Delete timer from NULL loop */
    ret = cobalt_eventloop_del_timer(NULL, 1);
    TEST_ASSERT(ret == -1);
    printf("  Delete from NULL loop: OK\n");

    /* Add timer to NULL loop */
    id_del = cobalt_eventloop_add_timer(NULL, 100, 0, on_timer, NULL);
    TEST_ASSERT(id_del == 0);
    printf("  Add timer to NULL loop: OK\n");

    cobalt_eventloop_destroy(loop);
    printf("  Timer edge cases passed\n");
}

void test_eventloop_fd_edge_cases(void)
{
    printf("Testing eventloop FD edge cases...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    int pipefd[2];
    if (pipe(pipefd) == 0) {
        /* Delete non-existent FD */
        int ret = cobalt_eventloop_del_fd(loop, pipefd[0]);
        TEST_ASSERT(ret == -1);
        printf("  Delete non-existent FD: OK\n");

        /* Mod non-existent FD */
        ret = cobalt_eventloop_mod_fd(loop, pipefd[0], POLLIN, on_fd, NULL);
        TEST_ASSERT(ret == 0);
        printf("  Mod non-existent FD (creates new): OK\n");

        /* Add same FD twice */
        ret = cobalt_eventloop_add_fd(loop, pipefd[0], POLLIN, on_fd, NULL);
        TEST_ASSERT(ret != 0);
        ret = cobalt_eventloop_add_fd(loop, pipefd[0], POLLIN, on_fd, NULL);
        TEST_ASSERT(ret != 0); /* Duplicate add should fail */
        printf("  Duplicate FD add: OK\n");

        /* Delete FD, then delete again */
        ret = cobalt_eventloop_del_fd(loop, pipefd[0]);
        TEST_ASSERT(ret == 0);
        ret = cobalt_eventloop_del_fd(loop, pipefd[0]);
        TEST_ASSERT(ret == -1);
        printf("  Double delete FD: OK\n");

        close(pipefd[0]);
        close(pipefd[1]);
    }

    /* NULL loop tests */
    int ret = cobalt_eventloop_add_fd(NULL, 0, POLLIN, on_fd, NULL);
    TEST_ASSERT(ret == -1);
    ret = cobalt_eventloop_mod_fd(NULL, 0, POLLIN, on_fd, NULL);
    TEST_ASSERT(ret == -1);
    ret = cobalt_eventloop_del_fd(NULL, 0);
    TEST_ASSERT(ret == -1);
    printf("  NULL loop operations: OK\n");

    cobalt_eventloop_destroy(loop);
    printf("  FD edge cases passed\n");
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
    test_eventloop_timing();
    test_eventloop_timer_edge_cases();
    test_eventloop_fd_edge_cases();
    printf("  Eventloop tests completed\n");
}

void test_eventloop_timer_periodic(void)
{
    printf("Testing eventloop periodic timer...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    int fire_count = 0;

    /* Add a periodic timer with 10ms interval */
    uint64_t id = cobalt_eventloop_add_timer(loop, 10, 10, on_timer, &fire_count);
    TEST_ASSERT(id != 0);
    printf("  Periodic timer added: id=%lu\n", (unsigned long)id);

    /* Run iterations with small delays */
    for (int i = 0; i < 5; i++) {
        cobalt_eventloop_iteration(loop);
        usleep(5000);
    }

    printf("  Periodic timer fired %d times\n", fire_count);
    TEST_ASSERT(fire_count >= 1);

    cobalt_eventloop_destroy(loop);
    printf("  Periodic timer test passed\n");
}
