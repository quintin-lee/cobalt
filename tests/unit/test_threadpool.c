/**
 * @file test_threadpool.c
 * @brief Unit tests for work-stealing thread pool and scheduler
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

    /* Drain remainder after stop signal */
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

    /* Owner thread pushes and occasionally pops */
    int popped_by_owner = 0;
    for (int i = 0; i < num_items; i++) {
        while (cobalt_test_chase_lev_push(q, dummy_task_fn, (void *)(uintptr_t)i) != 0) {
            /* Deque full, owner pops one */
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

        /* 20% of the time, owner pops to create owner vs stealer races */
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

    /* Owner pops until queue is empty */
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

    /* Verify every item [0..num_items-1] was popped or stolen EXACTLY once */
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
/* Main Module Test Entry                                                     */
/* -------------------------------------------------------------------------- */

void test_threadpool(void)
{
    printf("Testing threadpool module...\n");
    test_chase_lev_single_thread();
    test_chase_lev_steal_fifo();
    test_chase_lev_concurrent_steal();
    printf("  Threadpool tests completed\n");
}
