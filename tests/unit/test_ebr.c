/**
 * @file test_ebr.c
 * @brief Unit tests for Layer 7 Epoch-Based Reclamation (EBR) engine
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "cobalt/memory/allocator.h"
#include "cobalt/memory/ebr.h"
#include "cobalt/platform/thread.h"
#include "test_framework.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int         value;
    atomic_int *free_counter;
} ebr_test_payload_t;

static void ebr_test_payload_free(void *ptr, void *user_data)
{
    (void)user_data;
    ebr_test_payload_t *item = (ebr_test_payload_t *)ptr;
    if (item && item->free_counter) {
        atomic_fetch_add(item->free_counter, 1);
    }
    free(item);
}

static void test_ebr_create_destroy(void)
{
    printf("Testing EBR create and destroy...\n");

    cobalt_ebr_t *ebr = cobalt_ebr_create(NULL);
    TEST_ASSERT(ebr != NULL);

    cobalt_ebr_handle_t *h1 = cobalt_ebr_register(ebr);
    TEST_ASSERT(h1 != NULL);

    cobalt_ebr_handle_t *h2 = cobalt_ebr_register(ebr);
    TEST_ASSERT(h2 != NULL);
    TEST_ASSERT(h1 != h2);

    cobalt_ebr_destroy(ebr);
    cobalt_ebr_destroy(NULL); /* Should not crash */

    /* Custom allocator test */
    cobalt_allocator_t *alloc = cobalt_allocator_get_system();
    ebr                       = cobalt_ebr_create(alloc);
    TEST_ASSERT(ebr != NULL);
    cobalt_ebr_destroy(ebr);

    printf("  ebr create/destroy: OK\n");
}

static void test_ebr_enter_exit(void)
{
    printf("Testing EBR enter and exit...\n");

    cobalt_ebr_t *ebr = cobalt_ebr_create(NULL);
    TEST_ASSERT(ebr != NULL);

    cobalt_ebr_handle_t *h = cobalt_ebr_register(ebr);
    TEST_ASSERT(h != NULL);

    cobalt_ebr_enter(h);
    cobalt_ebr_exit(h);

    cobalt_ebr_unregister(h);
    cobalt_ebr_destroy(ebr);

    printf("  ebr enter/exit: OK\n");
}

static void test_ebr_delayed_reclaim(void)
{
    printf("Testing EBR 3-epoch delayed reclamation invariant...\n");

    cobalt_ebr_t *ebr = cobalt_ebr_create(NULL);
    TEST_ASSERT(ebr != NULL);

    cobalt_ebr_handle_t *h = cobalt_ebr_register(ebr);
    TEST_ASSERT(h != NULL);

    atomic_int free_count = 0;

    ebr_test_payload_t *item = (ebr_test_payload_t *)malloc(sizeof(ebr_test_payload_t));
    TEST_ASSERT(item != NULL);
    item->value        = 42;
    item->free_counter = &free_count;

    /* Enter epoch, retire item, then exit */
    cobalt_ebr_enter(h);
    cobalt_ebr_retire(h, item, ebr_test_payload_free, NULL);
    cobalt_ebr_exit(h);

    /* Immediately after retire, item MUST NOT be freed */
    TEST_EQUAL(atomic_load(&free_count), 0);

    /* Advance epoch 1 time: epoch moves from 0 to 1. Still MUST NOT be freed (only 1 epoch passed)
     */
    TEST_ASSERT(cobalt_ebr_try_advance(ebr));
    cobalt_ebr_enter(h);
    cobalt_ebr_exit(h);
    TEST_EQUAL(atomic_load(&free_count), 0);

    /* Advance epoch 2nd time: epoch moves from 1 to 2. Item from epoch 0 can now be freed! */
    TEST_ASSERT(cobalt_ebr_try_advance(ebr));

    /* Enter/exit to trigger local maintenance reclaim */
    cobalt_ebr_enter(h);
    cobalt_ebr_exit(h);

    /* Invariant verified: item is strictly freed after 2 full epoch advances! */
    TEST_EQUAL(atomic_load(&free_count), 1);

    cobalt_ebr_unregister(h);
    cobalt_ebr_destroy(ebr);

    printf("  ebr delayed reclamation: OK\n");
}

typedef struct {
    cobalt_ebr_t *ebr;
    int           thread_id;
    int           iterations;
    atomic_int   *total_freed;
} ebr_worker_arg_t;

static void *ebr_stress_worker(void *arg)
{
    ebr_worker_arg_t    *w = (ebr_worker_arg_t *)arg;
    cobalt_ebr_handle_t *h = cobalt_ebr_register(w->ebr);
    TEST_ASSERT(h != NULL);

    for (int i = 0; i < w->iterations; i++) {
        cobalt_ebr_enter(h);

        /* Simulate allocation and retirement */
        ebr_test_payload_t *item = (ebr_test_payload_t *)malloc(sizeof(ebr_test_payload_t));
        item->value              = i;
        item->free_counter       = w->total_freed;
        cobalt_ebr_retire(h, item, ebr_test_payload_free, NULL);

        cobalt_ebr_exit(h);

        if (i % 50 == 0) {
            cobalt_ebr_try_advance(w->ebr);
        }
    }

    cobalt_ebr_unregister(h);
    return NULL;
}

static void test_ebr_multithread_advance(void)
{
    printf("Testing EBR multi-thread concurrent advance and retirement...\n");

    cobalt_ebr_t *ebr = cobalt_ebr_create(NULL);
    TEST_ASSERT(ebr != NULL);

    const int  num_threads      = 4;
    const int  iters_per_thread = 500;
    atomic_int total_freed      = 0;

    cobalt_thread_t  threads[4];
    ebr_worker_arg_t args[4];

    for (int i = 0; i < num_threads; i++) {
        args[i].ebr         = ebr;
        args[i].thread_id   = i;
        args[i].iterations  = iters_per_thread;
        args[i].total_freed = &total_freed;
        TEST_EQUAL(cobalt_thread_create(ebr_stress_worker, &args[i], &threads[i]), 0);
    }

    for (int i = 0; i < num_threads; i++) {
        cobalt_thread_join(threads[i]);
    }

    /* Force full sync to reclaim all remaining retired items */
    cobalt_ebr_sync(ebr);

    /* Verify all retired nodes were cleanly freed */
    int expected_freed = num_threads * iters_per_thread;
    TEST_EQUAL(atomic_load(&total_freed), expected_freed);

    cobalt_ebr_destroy(ebr);
    printf("  ebr multithread advance (%d items freed): OK\n", expected_freed);
}

static void test_ebr_handle_reuse(void)
{
    printf("Testing EBR handle reuse across thread registrations...\n");

    cobalt_ebr_t *ebr = cobalt_ebr_create(NULL);
    TEST_ASSERT(ebr != NULL);

    cobalt_ebr_handle_t *h1 = cobalt_ebr_register(ebr);
    TEST_ASSERT(h1 != NULL);

    cobalt_ebr_unregister(h1);

    /* Next register should reuse h1 */
    cobalt_ebr_handle_t *h2 = cobalt_ebr_register(ebr);
    TEST_ASSERT(h2 != NULL);
    TEST_EQUAL(h1, h2);

    cobalt_ebr_unregister(h2);
    cobalt_ebr_destroy(ebr);

    printf("  ebr handle reuse: OK\n");
}

void test_ebr(void)
{
    test_ebr_create_destroy();
    test_ebr_enter_exit();
    test_ebr_delayed_reclaim();
    test_ebr_multithread_advance();
    test_ebr_handle_reuse();
}
