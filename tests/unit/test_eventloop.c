/**
 * @file test_eventloop.c
 * @brief Unit test for event loop module.
 */

#include "cobalt/module/eventloop.h"
#include "test_framework.h"
#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

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
void test_eventloop_unix_socket(void);
void test_eventloop_timer_periodic(void);
void test_eventloop_rapid_signals(void);

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
#ifndef _WIN32
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
#else
    printf("  Skipping FD test on Windows (POSIX-only)\n");
#endif
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

    uint64_t id1 = cobalt_eventloop_add_timer(loop, 0, 0, on_timer, &counter1);
    uint64_t id2 = cobalt_eventloop_add_timer(loop, 0, 0, on_timer, &counter2);

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

    /* Add timers with same timeout so heap ordering is determined by timer_id (FIFO) */
    for (int i = 0; i < timer_count; i++) {
        timer_ids[i + 1] = cobalt_eventloop_add_timer(loop, 0, 0, on_timer, &timer_ids[i + 1]);
        TEST_ASSERT(timer_ids[i + 1] != 0);
    }

    /* Save original IDs before callbacks increment them */
    static uint64_t original_ids[101];
    for (int i = 0; i <= timer_count; i++) {
        original_ids[i] = timer_ids[i];
    }

    /* All timers expire immediately; first iteration processes all of them */
    cobalt_eventloop_iteration(loop);

    TEST_ASSERT(g_fire_idx == timer_count);

    /* Verify timers fire in ascending timer_id order (FIFO for same-time timers) */
    for (int i = 0; i < timer_count; i++) {
        TEST_ASSERT(fired_order[i] == (int)original_ids[i + 1]);
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

static void test_signal_handler(cobalt_fd_t fd, cobalt_events_t events, void *ud)
{
    (void)fd;
    (void)events;
    int *flag = (int *)ud;
    if (flag) {
        *flag = 1;
    }
}

static void test_close_handler(cobalt_fd_t fd, cobalt_events_t events, void *ud)
{
    (void)fd;
    (void)events;
    int *flag = (int *)ud;
    if (flag) {
        *flag = 1;
    }
}

void test_eventloop_signal(void)
{
#ifndef _WIN32
    printf("Testing eventloop signal handling...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    int signal_fired = 0;

    /* Register SIGUSR1 handler */
    int ret = cobalt_eventloop_add_signal(loop, SIGUSR1, test_signal_handler, &signal_fired);
    TEST_ASSERT(ret == 0);
    printf("  Signal handler registered: OK\n");

    /* Duplicate registration should fail */
    ret = cobalt_eventloop_add_signal(loop, SIGUSR1, test_signal_handler, &signal_fired);
    TEST_ASSERT(ret == -1);
    printf("  Duplicate signal reject: OK\n");

    /* NULL loop should fail */
    ret = cobalt_eventloop_add_signal(NULL, SIGUSR1, test_signal_handler, &signal_fired);
    TEST_ASSERT(ret == -1);
    printf("  NULL loop signal reject: OK\n");

    /* Send signal and drain */
    kill(getpid(), SIGUSR1);
    usleep(5000);
    cobalt_eventloop_iteration(loop);

    printf("  Signal test completed\n");
    cobalt_eventloop_destroy(loop);
#else
    printf("  Skipping signal test on Windows (POSIX-only)\n");
#endif
}

void test_eventloop_close_callback(void)
{
    printf("Testing eventloop close callback...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    int close_fired = 0;

    int ret = cobalt_eventloop_add_close_callback(loop, test_close_handler, &close_fired);
    TEST_ASSERT(ret == 0);
    printf("  Close callback registered: OK\n");

    /* Destroy should trigger close callback */
    cobalt_eventloop_destroy(loop);
    TEST_ASSERT(close_fired == 1);

    printf("  Close callback: OK\n");
}

static int g_rapid_signal_count = 0;

static void rapid_signal_handler(cobalt_fd_t fd, cobalt_events_t events, void *ud)
{
    (void)fd;
    (void)events;
    g_rapid_signal_count++;
}

void test_eventloop_rapid_signals(void)
{
#ifndef _WIN32
    printf("Testing eventloop rapid signals...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    g_rapid_signal_count = 0;

    int ret = cobalt_eventloop_add_signal(loop, SIGUSR2, rapid_signal_handler, NULL);
    TEST_ASSERT(ret == 0);

    /* Send multiple signals rapidly */
    for (int i = 0; i < 5; i++) {
        kill(getpid(), SIGUSR2);
    }
    usleep(10000);
    cobalt_eventloop_iteration(loop);

    printf("  Rapid signals: %d processed\n", g_rapid_signal_count);
    TEST_ASSERT(g_rapid_signal_count == 5);

    cobalt_eventloop_destroy(loop);
    printf("  Rapid signals test completed\n");
#else
    printf("  Skipping rapid signals test on Windows (POSIX-only)\n");
#endif
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
    test_eventloop_signal();
    test_eventloop_close_callback();
    test_eventloop_unix_socket();
    test_eventloop_timer_periodic();
    test_eventloop_rapid_signals();
    printf("  Eventloop tests completed\n");
}

void test_eventloop_timer_periodic(void)
{
    printf("Testing eventloop periodic timer...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    uint64_t fire_count = 0;

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

    cobalt_eventloop_del_timer(loop, id);
    cobalt_eventloop_destroy(loop);
    printf("  Periodic timer test passed\n");
}

/* Server callback for UNIX socket test — file-scope to avoid nested functions */
typedef struct {
    int accept_count;
} unix_test_ctx_t;

static void unix_server_cb(cobalt_fd_t fd, cobalt_events_t events, void *ud)
{
    (void)events;
    unix_test_ctx_t   *ctx     = (unix_test_ctx_t *)ud;
    struct sockaddr_un addr    = {};
    socklen_t          addrlen = sizeof(addr);
    cobalt_fd_t        cfd     = accept(fd, (struct sockaddr *)&addr, &addrlen);
    if (cfd >= 0) {
        ctx->accept_count++;
        close(cfd);
    }
}

void test_eventloop_unix_socket(void)
{
#ifndef _WIN32
    printf("Testing eventloop UNIX domain socket...\n");

    const char *sock_path = "/tmp/cobalt_unix_test.sock";
    unlink(sock_path);

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    /* Create server socket */
    cobalt_fd_t server_fd;
    int         ret = cobalt_eventloop_create_unix_server(sock_path, &server_fd);
    TEST_ASSERT(ret == 0);
    TEST_ASSERT(server_fd >= 0);

    /* Register server for incoming connections */
    unix_test_ctx_t ctx = {0};
    ret                 = cobalt_eventloop_add_fd(loop, server_fd, POLLIN, unix_server_cb, &ctx);
    TEST_ASSERT(ret == 0);

    /* Create client and connect */
    cobalt_fd_t client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    TEST_ASSERT(client_fd >= 0);

    struct sockaddr_un cli_addr = {};
    cli_addr.sun_family         = AF_UNIX;
    strncpy(cli_addr.sun_path, sock_path, sizeof(cli_addr.sun_path) - 1);

    ret = connect(client_fd, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
    TEST_ASSERT(ret == 0);

    /* Run one iteration to trigger accept */
    ret = cobalt_eventloop_iteration(loop);
    TEST_ASSERT(ret == 0);

    /* Server should have accepted the connection */
    TEST_ASSERT(ctx.accept_count > 0);

    /* Clean up */
    close(client_fd);
    cobalt_eventloop_del_fd(loop, server_fd);
    cobalt_eventloop_destroy(loop);
    unlink(sock_path);

    printf("  UNIX socket test passed\n");
#else
    printf("  Skipping UNIX socket test on Windows (POSIX-only)\n");
#endif
}

