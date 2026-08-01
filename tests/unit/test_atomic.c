/**
 * @file test_atomic.c
 * @Unit test for atomic operations.
 * Tests thread-safety of atomic increment/decrement using multiple threads.
 */

#include "cobalt/platform/atomic.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/* Shared data for the test */
static cobalt_atomic_t g_counter;
static int g_threads = 8;
static int increments_per_thread = 10000;

/* Thread function: increment the counter many times */
void* thread_inc(void* arg)
{
    (void)arg;
    for (int i = 0; i < increments_per_thread; i++)
        {
            cobalt_atomic_increment(&g_counter);
        }
    return NULL;
}

/* Thread function: decrement the counter many times */
void* thread_dec(void* arg)
{
    (void)arg;
    for (int i = 0; i < increments_per_thread; i++)
        {
            cobalt_atomic_decrement(&g_counter);
        }
    return NULL;
}

/* Test basic create/get/set */
void test_atomic_basic(void)
{
    printf("Testing atomic basic operations...\n");

    cobalt_atomic_t a = cobalt_atomic_create(42);
    int val = cobalt_atomic_get(&a);
    if (val == 42)
        {
            printf("  Create + get initial value: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: expected 42, got %d\n", val);
        }

    cobalt_atomic_set(&a, 100);
    val = cobalt_atomic_get(&a);
    if (val == 100)
        {
            printf("  Set + get: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: expected 100, got %d\n", val);
        }

    cobalt_atomic_increment(&a);
    val = cobalt_atomic_get(&a);
    if (val == 101)
        {
            printf("  Increment: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: expected 101, got %d\n", val);
        }

    cobalt_atomic_decrement(&a);
    val = cobalt_atomic_get(&a);
    if (val == 100)
        {
            printf("  Decrement: OK\n");
        }
    else
        {
            fprintf(stderr, "ERROR: expected 100, got %d\n", val);
        }
}

/* Test thread safety with concurrent increments */
void test_atomic_concurrent_inc(void)
{
    printf("Testing atomic concurrent increment (%d threads x %d iters)...\n", g_threads,
           increments_per_thread);

    /* Initialize counter */
    g_counter = cobalt_atomic_create(0);

    /* Create threads */
    pthread_t threads[g_threads];
    for (int i = 0; i < g_threads; i++)
        {
            int ret = pthread_create(&threads[i], NULL, thread_inc, NULL);
            if (ret != 0)
                {
                    fprintf(stderr, "ERROR: Failed to create thread %d\n", i);
                    return;
                }
        }

    /* Wait for all threads */
    for (int i = 0; i < g_threads; i++)
        {
            pthread_join(threads[i], NULL);
        }

    /* Check result */
    int final = cobalt_atomic_get(&g_counter);
    int expected = g_threads * increments_per_thread;
    if (final == expected)
        {
            printf("  Final counter %d (expected %d): OK\n", final, expected);
        }
    else
        {
            fprintf(stderr, "ERROR: Expected %d but got %d (race condition detected!)\n", expected,
                    final);
        }
}

/* Test thread safety with mixed inc/dec */
void test_atomic_concurrent_mixed(void)
{
    printf("Testing atomic concurrent mixed operations...\n");

    /* Initialize counter */
    g_counter = cobalt_atomic_create(0);

    /* Create alternating inc/dec threads */
    pthread_t threads[g_threads * 2];
    for (int i = 0; i < g_threads; i++)
        {
            pthread_create(&threads[i], NULL, thread_inc, NULL);
            pthread_create(&threads[i + g_threads], NULL, thread_dec, NULL);
        }

    /* Wait for all threads */
    for (int i = 0; i < g_threads * 2; i++)
        {
            pthread_join(threads[i], NULL);
        }

    /* Result should be zero (equal number of inc and dec) */
    int final = cobalt_atomic_get(&g_counter);
    if (final == 0)
        {
            printf("  Final counter %d (should be 0): OK\n", final);
        }
    else
        {
            fprintf(stderr, "ERROR: Expected 0 but got %d\n", final);
        }
}

void test_atomic(void)
{
    printf("Testing atomic...\n");

    test_atomic_basic();
    test_atomic_concurrent_inc();
    test_atomic_concurrent_mixed();

    printf("  Atomic tests completed\n");
}
