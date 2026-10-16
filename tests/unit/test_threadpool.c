/**
 * @file test_threadpool.c
 * @brief Comprehensive unit tests for work-stealing thread pool and scheduler
 */

#include "cobalt/module/threadpool.h"
#include "cobalt/platform/thread.h"
#include "test_framework.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations of test helpers implemented in threadpool.c */
int    cobalt_test_chase_lev_init(void **deque_out, size_t capacity);
void   cobalt_test_chase_lev_destroy(void *deque_ptr);
int    cobalt_test_chase_lev_push(void *deque_ptr, void (*fn)(void *), void *arg);
int    cobalt_test_chase_lev_pop(void *deque_ptr, void (**out_fn)(void *), void **out_arg);
int    cobalt_test_chase_lev_steal(void *deque_ptr, void (**out_fn)(void *), void **out_arg);
size_t cobalt_test_chase_lev_size(void *deque_ptr);

static void dummy_task_fn(void *arg)
{
    (void)arg;
}

/* -------------------------------------------------------------------------- */
/* Chase-Lev Deque Unit Tests                                                 */
/* -------------------------------------------------------------------------- */

void test_chase_lev_single_thread(void)
{
    printf("Testing Chase-Lev deque single-thread LIFO...\n");
    void *q = NULL;
    TEST_ASSERT(cobalt_test_chase_lev_init(&q, 4) == 0);
    TEST_ASSERT(q != NULL);
    TEST_EQUAL(cobalt_test_chase_lev_size(q), 0U);

    /* Push 4 items */
    TEST_ASSERT(cobalt_test_chase_lev_push(q, dummy_task_fn, (void *)10) == 0);
    TEST_ASSERT(cobalt_test_chase_lev_push(q, dummy_task_fn, (void *)20) == 0);
    TEST_ASSERT(cobalt_test_chase_lev_push(q, dummy_task_fn, (void *)30) == 0);
    TEST_ASSERT(cobalt_test_chase_lev_push(q, dummy_task_fn, (void *)40) == 0);
    TEST_EQUAL(cobalt_test_chase_lev_size(q), 4U);

    /* Deque should be full */
    TEST_ASSERT(cobalt_test_chase_lev_push(q, dummy_task_fn, (void *)50) == -1);

    /* Pop LIFO (most recent first): 40, 30, 20, 10 */
    void (*fn)(void *) = NULL;
    void *arg          = NULL;

    TEST_ASSERT(cobalt_test_chase_lev_pop(q, &fn, &arg) == 0);
    TEST_EQUAL((uintptr_t)arg, 40U);
    TEST_ASSERT(cobalt_test_chase_lev_pop(q, &fn, &arg) == 0);
    TEST_EQUAL((uintptr_t)arg, 30U);
    TEST_ASSERT(cobalt_test_chase_lev_pop(q, &fn, &arg) == 0);
    TEST_EQUAL((uintptr_t)arg, 20U);
    TEST_ASSERT(cobalt_test_chase_lev_pop(q, &fn, &arg) == 0);
    TEST_EQUAL((uintptr_t)arg, 10U);

    /* Deque should now be empty */
    TEST_ASSERT(cobalt_test_chase_lev_pop(q, &fn, &arg) == -1);
    TEST_EQUAL(cobalt_test_chase_lev_size(q), 0U);

    cobalt_test_chase_lev_destroy(q);
    printf("  chase_lev single-thread: OK\n");
}

void test_chase_lev_steal_fifo(void)
{
    printf("Testing Chase-Lev deque steal FIFO...\n");
    void *q = NULL;
    TEST_ASSERT(cobalt_test_chase_lev_init(&q, 4) == 0);

    /* Push 4 items: 1, 2, 3, 4 */
    TEST_ASSERT(cobalt_test_chase_lev_push(q, dummy_task_fn, (void *)1) == 0);
    TEST_ASSERT(cobalt_test_chase_lev_push(q, dummy_task_fn, (void *)2) == 0);
    TEST_ASSERT(cobalt_test_chase_lev_push(q, dummy_task_fn, (void *)3) == 0);
    TEST_ASSERT(cobalt_test_chase_lev_push(q, dummy_task_fn, (void *)4) == 0);

    /* Steal FIFO (oldest first): 1, 2, 3, 4 */
    void (*fn)(void *) = NULL;
    void *arg          = NULL;

    TEST_ASSERT(cobalt_test_chase_lev_steal(q, &fn, &arg) == 0);
    TEST_EQUAL((uintptr_t)arg, 1U);
    TEST_ASSERT(cobalt_test_chase_lev_steal(q, &fn, &arg) == 0);
    TEST_EQUAL((uintptr_t)arg, 2U);
    TEST_ASSERT(cobalt_test_chase_lev_steal(q, &fn, &arg) == 0);
    TEST_EQUAL((uintptr_t)arg, 3U);
    TEST_ASSERT(cobalt_test_chase_lev_steal(q, &fn, &arg) == 0);
    TEST_EQUAL((uintptr_t)arg, 4U);

    /* Deque should now be empty */
    TEST_ASSERT(cobalt_test_chase_lev_steal(q, &fn, &arg) == -1);

    cobalt_test_chase_lev_destroy(q);
    printf("  chase_lev steal FIFO: OK\n");
}

typedef struct {
    void       *deque;
    atomic_bool stop;
    atomic_int *seen_counts;
    atomic_int  total_stolen;
} stealer_ctx_t;

static void *stealer_thread_fn(void *arg)
{
    stealer_ctx_t *ctx = (stealer_ctx_t *)arg;
    void (*fn)(void *) = NULL;
    void *task_arg     = NULL;

    while (!atomic_load_explicit(&ctx->stop, memory_order_relaxed)) {
        if (cobalt_test_chase_lev_steal(ctx->deque, &fn, &task_arg) == 0) {
            uintptr_t val = (uintptr_t)task_arg;
            atomic_fetch_add(&ctx->seen_counts[val], 1);
            atomic_fetch_add(&ctx->total_stolen, 1);
        } else {
            cobalt_thread_yield();
        }
    }

    while (cobalt_test_chase_lev_steal(ctx->deque, &fn, &task_arg) == 0) {
        uintptr_t val = (uintptr_t)task_arg;
        atomic_fetch_add(&ctx->seen_counts[val], 1);
        atomic_fetch_add(&ctx->total_stolen, 1);
    }

    return NULL;
}

void test_chase_lev_concurrent_steal(void)
{
    printf("Testing Chase-Lev concurrent owner pop vs multi-stealer...\n");
    void *q = NULL;
    TEST_ASSERT(cobalt_test_chase_lev_init(&q, 1024) == 0);

    const int   num_items = 5000;
    atomic_int *seen      = calloc(num_items, sizeof(atomic_int));
    TEST_ASSERT(seen != NULL);

    stealer_ctx_t ctx;
    ctx.deque = q;
    atomic_init(&ctx.stop, false);
    ctx.seen_counts = seen;
    atomic_init(&ctx.total_stolen, 0);

    const int       num_stealers = 3;
    cobalt_thread_t stealers[3];
    for (int i = 0; i < num_stealers; i++) {
        TEST_ASSERT(cobalt_thread_create(stealer_thread_fn, &ctx, &stealers[i]) == 0);
    }

    int popped_by_owner = 0;
    for (int i = 0; i < num_items; i++) {
        while (cobalt_test_chase_lev_push(q, dummy_task_fn, (void *)(uintptr_t)i) != 0) {
            void (*fn)(void *) = NULL;
            void *task_arg     = NULL;
            if (cobalt_test_chase_lev_pop(q, &fn, &task_arg) == 0) {
                uintptr_t val = (uintptr_t)task_arg;
                atomic_fetch_add(&seen[val], 1);
                popped_by_owner++;
            } else {
                cobalt_thread_yield();
            }
        }

        if (i % 5 == 0) {
            void (*fn)(void *) = NULL;
            void *task_arg     = NULL;
            if (cobalt_test_chase_lev_pop(q, &fn, &task_arg) == 0) {
                uintptr_t val = (uintptr_t)task_arg;
                atomic_fetch_add(&seen[val], 1);
                popped_by_owner++;
            }
        }
    }

    void (*fn)(void *) = NULL;
    void *task_arg     = NULL;
    while (cobalt_test_chase_lev_pop(q, &fn, &task_arg) == 0) {
        uintptr_t val = (uintptr_t)task_arg;
        atomic_fetch_add(&seen[val], 1);
        popped_by_owner++;
    }

    atomic_store(&ctx.stop, true);
    for (int i = 0; i < num_stealers; i++) {
        cobalt_thread_join(stealers[i]);
    }

    int total_accounted = 0;
    for (int i = 0; i < num_items; i++) {
        int c = atomic_load(&seen[i]);
        TEST_EQUAL(c, 1);
        total_accounted += c;
    }
    TEST_EQUAL(total_accounted, num_items);

    free(seen);
    cobalt_test_chase_lev_destroy(q);
    printf("  chase_lev concurrent steal: OK (owner: %d, stealers: %d)\n",
           popped_by_owner,
           atomic_load(&ctx.total_stolen));
}

/* -------------------------------------------------------------------------- */
/* Thread Pool Lifecycle & Task Submission Tests                              */
/* -------------------------------------------------------------------------- */

static void count_increment_task(void *arg)
{
    atomic_int *counter = (atomic_int *)arg;
    atomic_fetch_add(counter, 1);
}

void test_threadpool_lifecycle(void)
{
    printf("Testing threadpool lifecycle and auto-detect...\n");

    /* Auto-detect CPU cores */
    cobalt_threadpool_t *pool = cobalt_threadpool_create(0);
    TEST_ASSERT(pool != NULL);
    size_t cores = cobalt_threadpool_thread_count(pool);
    TEST_ASSERT(cores > 0);
    TEST_EQUAL(cobalt_threadpool_active_tasks(pool), 0U);

    cobalt_threadpool_destroy(pool);
    cobalt_threadpool_destroy(NULL); /* Should not crash */

    /* Fixed worker count */
    pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);
    TEST_EQUAL(cobalt_threadpool_thread_count(pool), 4U);

    atomic_int counter;
    atomic_init(&counter, 0);

    /* Submit 100 tasks */
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT(cobalt_threadpool_submit(pool, count_increment_task, &counter) == 0);
    }

    cobalt_threadpool_wait_idle(pool);
    TEST_EQUAL(atomic_load(&counter), 100);
    TEST_EQUAL(cobalt_threadpool_active_tasks(pool), 0U);

    cobalt_threadpool_destroy(pool);
    printf("  threadpool lifecycle: OK (%zu cores detected)\n", cores);
}

void test_threadpool_concurrent_submit(void)
{
    printf("Testing threadpool 20,000 tasks concurrent execution...\n");
    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);

    atomic_int counter;
    atomic_init(&counter, 0);
    const int total_tasks = 20000;

    for (int i = 0; i < total_tasks; i++) {
        TEST_ASSERT(cobalt_threadpool_submit(pool, count_increment_task, &counter) == 0);
    }

    cobalt_threadpool_wait_idle(pool);
    TEST_EQUAL(atomic_load(&counter), total_tasks);

    cobalt_threadpool_destroy(pool);
    printf("  threadpool concurrent submit: OK (%d tasks)\n", total_tasks);
}

/* -------------------------------------------------------------------------- */
/* WaitGroup Tests                                                            */
/* -------------------------------------------------------------------------- */

typedef struct {
    atomic_int         *counter;
    cobalt_waitgroup_t *wg;
} wg_task_arg_t;

static void wg_increment_task(void *arg)
{
    wg_task_arg_t *a = (wg_task_arg_t *)arg;
    atomic_fetch_add(a->counter, 1);
    cobalt_waitgroup_done(a->wg);
}

void test_threadpool_waitgroup(void)
{
    printf("Testing WaitGroup synchronization...\n");
    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);

    cobalt_waitgroup_t *wg = cobalt_waitgroup_create();
    TEST_ASSERT(wg != NULL);

    atomic_int counter;
    atomic_init(&counter, 0);

    const int     task_count = 500;
    wg_task_arg_t args[500];

    cobalt_waitgroup_add(wg, task_count);
    for (int i = 0; i < task_count; i++) {
        args[i].counter = &counter;
        args[i].wg      = wg;
        TEST_ASSERT(cobalt_threadpool_submit(pool, wg_increment_task, &args[i]) == 0);
    }

    /* Wait for all 500 to complete */
    cobalt_waitgroup_wait(wg);
    TEST_EQUAL(atomic_load(&counter), task_count);

    cobalt_waitgroup_destroy(wg);
    cobalt_threadpool_destroy(pool);
    printf("  threadpool waitgroup: OK\n");
}

/* Nested recursive fork-join task testing help-first work-stealing */
typedef struct {
    int                  depth;
    cobalt_threadpool_t *pool;
    atomic_int          *leaf_counter;
    cobalt_waitgroup_t  *parent_wg;
    bool                 is_root;
} fork_join_arg_t;

static void recursive_fork_join_task(void *arg)
{
    fork_join_arg_t     *a            = (fork_join_arg_t *)arg;
    int                  depth        = a->depth;
    cobalt_threadpool_t *pool         = a->pool;
    atomic_int          *leaf_counter = a->leaf_counter;
    cobalt_waitgroup_t  *parent_wg    = a->parent_wg;
    if (!a->is_root) {
        free(a);
    }

    if (depth <= 0) {
        atomic_fetch_add(leaf_counter, 1);
        if (parent_wg) {
            cobalt_waitgroup_done(parent_wg);
        }
        return;
    }

    cobalt_waitgroup_t *wg = cobalt_waitgroup_create();
    cobalt_waitgroup_add(wg, 2);

    fork_join_arg_t *left = (fork_join_arg_t *)malloc(sizeof(fork_join_arg_t));
    *left                 = (fork_join_arg_t){.depth        = depth - 1,
                                              .pool         = pool,
                                              .leaf_counter = leaf_counter,
                                              .parent_wg    = wg,
                                              .is_root      = false};

    fork_join_arg_t *right = (fork_join_arg_t *)malloc(sizeof(fork_join_arg_t));
    *right                 = (fork_join_arg_t){.depth        = depth - 1,
                                               .pool         = pool,
                                               .leaf_counter = leaf_counter,
                                               .parent_wg    = wg,
                                               .is_root      = false};

    cobalt_threadpool_submit(pool, recursive_fork_join_task, left);
    cobalt_threadpool_submit(pool, recursive_fork_join_task, right);

    /* Worker calling waitgroup_wait will execute help-first work-stealing */
    cobalt_waitgroup_wait(wg);
    cobalt_waitgroup_destroy(wg);

    if (parent_wg) {
        cobalt_waitgroup_done(parent_wg);
    }
}

void test_threadpool_recursive_deadlock_free(void)
{
    printf("Testing recursive fork-join Help-First work-stealing...\n");
    /* Run with just 2 workers to test deadlock freedom on recursive fork-join */
    cobalt_threadpool_t *pool = cobalt_threadpool_create(2);
    TEST_ASSERT(pool != NULL);

    atomic_int leaves;
    atomic_init(&leaves, 0);

    /* Depth 6 binary tree = 2^6 = 64 leaves */
    fork_join_arg_t root = {
        .depth = 6, .pool = pool, .leaf_counter = &leaves, .parent_wg = NULL, .is_root = true};
    recursive_fork_join_task(&root);

    TEST_EQUAL(atomic_load(&leaves), 64);

    cobalt_threadpool_destroy(pool);
    printf("  recursive fork-join deadlock-free: OK (64 leaves)\n");
}

/* -------------------------------------------------------------------------- */
/* Parallel For Tests                                                         */
/* -------------------------------------------------------------------------- */

static void parallel_square_fn(size_t index, void *arg)
{
    uint64_t *array = (uint64_t *)arg;
    array[index]    = (uint64_t)index * 2ULL;
}

void test_threadpool_parallel_for(void)
{
    printf("Testing threadpool parallel_for on 100,000 items...\n");
    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);

    const size_t N   = 100000;
    uint64_t    *arr = (uint64_t *)malloc(N * sizeof(uint64_t));
    TEST_ASSERT(arr != NULL);
    memset(arr, 0, N * sizeof(uint64_t));

    /* Parallel For with auto chunk size (chunk_size = 0) */
    TEST_ASSERT(cobalt_threadpool_parallel_for(pool, 0, N, 0, parallel_square_fn, arr) == 0);

    /* Verify correctness */
    for (size_t i = 0; i < N; i++) {
        if (arr[i] != i * 2ULL) {
            TEST_ASSERT(arr[i] == i * 2ULL);
            break;
        }
    }

    /* Edge cases */
    TEST_ASSERT(cobalt_threadpool_parallel_for(pool, 10, 10, 0, parallel_square_fn, arr) == 0);
    TEST_ASSERT(cobalt_threadpool_parallel_for(pool, 20, 10, 0, parallel_square_fn, arr) == 0);
    TEST_ASSERT(cobalt_threadpool_parallel_for(NULL, 0, N, 0, parallel_square_fn, arr) == -1);
    TEST_ASSERT(cobalt_threadpool_parallel_for(pool, 0, N, 0, NULL, arr) == -1);

    free(arr);
    cobalt_threadpool_destroy(pool);
    printf("  parallel_for: OK\n");
}

/* -------------------------------------------------------------------------- */
/* Main Module Test Entry                                                     */
/* -------------------------------------------------------------------------- */

void test_threadpool(void)
{
    printf("Testing threadpool module...\n");
    test_chase_lev_single_thread();
    test_chase_lev_steal_fifo();
    test_chase_lev_concurrent_steal();
    test_threadpool_lifecycle();
    test_threadpool_concurrent_submit();
    test_threadpool_waitgroup();
    test_threadpool_recursive_deadlock_free();
    test_threadpool_parallel_for();
    printf("  Threadpool tests completed\n");
}
