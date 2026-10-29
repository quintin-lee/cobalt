/**
 * @file test_lfqueue.c
 * @brief Unit tests for bounded lock-free MPMC queue
 */

#include "cobalt/container/lfqueue.h"
#include "cobalt/platform/thread.h"
#include "test_framework.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_lfqueue_create_destroy(void)
{
    printf("Testing lfqueue create and destroy...\n");

    cobalt_lfqueue_t *q = cobalt_lfqueue_create(10);
    TEST_ASSERT(q != NULL);
    TEST_ASSERT(cobalt_lfqueue_capacity(q) == 16); /* rounded to next pow2 */
    TEST_ASSERT(cobalt_lfqueue_is_empty(q));
    TEST_ASSERT(!cobalt_lfqueue_is_full(q));
    TEST_ASSERT(cobalt_lfqueue_size(q) == 0);

    /* NULL checks */
    TEST_ASSERT(cobalt_lfqueue_enqueue(NULL, (void *)1) == -1);
    void *out = NULL;
    TEST_ASSERT(cobalt_lfqueue_dequeue(NULL, &out) == -1);
    TEST_ASSERT(cobalt_lfqueue_dequeue(q, NULL) == -1);
    TEST_ASSERT(cobalt_lfqueue_size(NULL) == 0);
    TEST_ASSERT(cobalt_lfqueue_capacity(NULL) == 0);
    TEST_ASSERT(cobalt_lfqueue_is_empty(NULL) == 1);
    TEST_ASSERT(cobalt_lfqueue_is_full(NULL) == 0);

    cobalt_lfqueue_destroy(q);
    cobalt_lfqueue_destroy(NULL); /* Should not crash */
    printf("  lfqueue create/destroy: OK\n");
}

void test_lfqueue_single_thread(void)
{
    printf("Testing lfqueue single-thread FIFO & boundary...\n");

    cobalt_lfqueue_t *q = cobalt_lfqueue_create(4);
    TEST_ASSERT(q != NULL);
    TEST_ASSERT(cobalt_lfqueue_capacity(q) == 4);

    uintptr_t val1 = 100, val2 = 200, val3 = 300, val4 = 400;
    TEST_ASSERT(cobalt_lfqueue_enqueue(q, (void *)val1) == 0);
    TEST_ASSERT(cobalt_lfqueue_enqueue(q, (void *)val2) == 0);
    TEST_ASSERT(cobalt_lfqueue_enqueue(q, (void *)val3) == 0);
    TEST_ASSERT(cobalt_lfqueue_enqueue(q, (void *)val4) == 0);

    /* Queue should be full */
    TEST_ASSERT(cobalt_lfqueue_is_full(q));
    TEST_ASSERT(cobalt_lfqueue_enqueue(q, (void *)500) == -1);
    TEST_ASSERT(cobalt_lfqueue_size(q) == 4);

    void *out = NULL;
    TEST_ASSERT(cobalt_lfqueue_dequeue(q, &out) == 0 && (uintptr_t)out == 100);
    TEST_ASSERT(cobalt_lfqueue_dequeue(q, &out) == 0 && (uintptr_t)out == 200);
    TEST_ASSERT(cobalt_lfqueue_dequeue(q, &out) == 0 && (uintptr_t)out == 300);
    TEST_ASSERT(cobalt_lfqueue_dequeue(q, &out) == 0 && (uintptr_t)out == 400);

    /* Queue should be empty */
    TEST_ASSERT(cobalt_lfqueue_is_empty(q));
    TEST_ASSERT(cobalt_lfqueue_size(q) == 0);
    TEST_ASSERT(cobalt_lfqueue_dequeue(q, &out) == -1);

    /* Multiple wrap-arounds */
    for (int cycle = 0; cycle < 100; cycle++) {
        for (int i = 0; i < 4; i++) {
            TEST_ASSERT(cobalt_lfqueue_enqueue(q, (void *)(uintptr_t)(cycle * 10 + i)) == 0);
        }
        for (int i = 0; i < 4; i++) {
            TEST_ASSERT(cobalt_lfqueue_dequeue(q, &out) == 0);
            TEST_ASSERT((uintptr_t)out == (uintptr_t)(cycle * 10 + i));
        }
    }
    TEST_ASSERT(cobalt_lfqueue_is_empty(q));

    cobalt_lfqueue_destroy(q);
    printf("  lfqueue single thread: OK\n");
}

#define LF_CONCURRENT_ITEMS 25000
#define LF_NUM_PRODUCERS 4
#define LF_NUM_CONSUMERS 4

typedef struct {
    cobalt_lfqueue_t *q;
    int               thread_id;
} lf_worker_arg_t;

static void *lf_producer_worker(void *arg)
{
    lf_worker_arg_t *w = (lf_worker_arg_t *)arg;
    for (int i = 0; i < LF_CONCURRENT_ITEMS; i++) {
        uintptr_t val = (uintptr_t)(w->thread_id * LF_CONCURRENT_ITEMS + i + 1);
        while (cobalt_lfqueue_enqueue(w->q, (void *)val) != 0) {
            cobalt_thread_yield();
        }
    }
    return NULL;
}

static uint64_t g_consumer_sums[LF_NUM_CONSUMERS];

static void *lf_consumer_worker(void *arg)
{
    lf_worker_arg_t *w            = (lf_worker_arg_t *)arg;
    uint64_t         sum          = 0;
    int              items_popped = 0;
    int              target_total = (LF_NUM_PRODUCERS * LF_CONCURRENT_ITEMS) / LF_NUM_CONSUMERS;

    while (items_popped < target_total) {
        void *data = NULL;
        if (cobalt_lfqueue_dequeue(w->q, &data) == 0) {
            sum += (uintptr_t)data;
            items_popped++;
        } else {
            cobalt_thread_yield();
        }
    }
    g_consumer_sums[w->thread_id] = sum;
    return NULL;
}

void test_lfqueue_multi_thread(void)
{
    printf("Testing lfqueue multi-threaded concurrency (4P + 4C)...\n");

    cobalt_lfqueue_t *q = cobalt_lfqueue_create(1024);
    TEST_ASSERT(q != NULL);

    cobalt_thread_t producers[LF_NUM_PRODUCERS];
    cobalt_thread_t consumers[LF_NUM_CONSUMERS];
    lf_worker_arg_t prod_args[LF_NUM_PRODUCERS];
    lf_worker_arg_t cons_args[LF_NUM_CONSUMERS];

    for (int i = 0; i < LF_NUM_CONSUMERS; i++) {
        cons_args[i].q         = q;
        cons_args[i].thread_id = i;
        g_consumer_sums[i]     = 0;
        TEST_ASSERT(cobalt_thread_create(lf_consumer_worker, &cons_args[i], &consumers[i]) == 0);
    }

    for (int i = 0; i < LF_NUM_PRODUCERS; i++) {
        prod_args[i].q         = q;
        prod_args[i].thread_id = i;
        TEST_ASSERT(cobalt_thread_create(lf_producer_worker, &prod_args[i], &producers[i]) == 0);
    }

    for (int i = 0; i < LF_NUM_PRODUCERS; i++) {
        cobalt_thread_join(producers[i]);
    }
    for (int i = 0; i < LF_NUM_CONSUMERS; i++) {
        cobalt_thread_join(consumers[i]);
    }

    uint64_t total_sum = 0;
    for (int i = 0; i < LF_NUM_CONSUMERS; i++) {
        total_sum += g_consumer_sums[i];
    }

    /* Expected sum of all produced values */
    uint64_t expected_sum = 0;
    for (int p = 0; p < LF_NUM_PRODUCERS; p++) {
        for (int i = 0; i < LF_CONCURRENT_ITEMS; i++) {
            expected_sum += (uint64_t)(p * LF_CONCURRENT_ITEMS + i + 1);
        }
    }
    TEST_ASSERT(total_sum == expected_sum);
    TEST_ASSERT(cobalt_lfqueue_is_empty(q));

    cobalt_lfqueue_destroy(q);
    printf("  lfqueue multi-threaded concurrency: OK\n");
}

void test_lfqueue_with_allocator(void)
{
    printf("Testing lfqueue with custom allocator...\n");

    /* Verified against lfqueue.c: NULL allocator is rejected, not defaulted */
    TEST_ASSERT(cobalt_lfqueue_create_with_allocator(8, NULL) == NULL);

    cobalt_test_counting_init();
    cobalt_lfqueue_t *q = cobalt_lfqueue_create_with_allocator(8, &cobalt_test_counting_v);
    TEST_ASSERT(q != NULL);
    /* Probe-verified: node slab + free-list, 2 allocations */
    TEST_EQUAL(cobalt_test_count_allocs, 2);

    int item = 7;
    TEST_ASSERT(cobalt_lfqueue_enqueue(q, &item) == 0);
    TEST_EQUAL(cobalt_lfqueue_size(q), (size_t)1);

    cobalt_lfqueue_destroy(q);
    printf("  Lfqueue allocator injection boundary: OK\n");
}

void test_lfqueue(void)
{
    printf("Testing lfqueue...\n");
    test_lfqueue_create_destroy();
    test_lfqueue_single_thread();
    test_lfqueue_multi_thread();
    test_lfqueue_with_allocator();
    printf("  lfqueue tests completed\n");
}
