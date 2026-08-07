/**
 * @file concurrent_demo.c
 * @brief Demonstrates thread-safe operations with Cobalt
 *
 * Shows mutex, thread creation, and synchronized counter usage.
 */

#include <cobalt/cobalt.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 4
#define ITERATIONS 1000

static cobalt_mutex_t *counter_mutex  = NULL;
static int             shared_counter = 0;

static void *thread_func(void *arg)
{
    int tid = *(int *)arg;
    free(arg);

    for (int i = 0; i < ITERATIONS; i++) {
        cobalt_mutex_lock(counter_mutex);
        shared_counter++;
        cobalt_mutex_unlock(counter_mutex);
    }

    cobalt_info("Thread %d completed %d iterations\n", tid, ITERATIONS);
    return NULL;
}

int main(void)
{
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    printf("=== Concurrent Demo ===\n\n");

    /* Create mutex */
    counter_mutex = cobalt_mutex_create();
    if (!counter_mutex) {
        fprintf(stderr, "Failed to create mutex\n");
        return 1;
    }

    /* Create threads */
    cobalt_thread_t threads[NUM_THREADS];

    cobalt_info("Creating %d threads, each doing %d increments\n", NUM_THREADS, ITERATIONS);

    for (int i = 0; i < NUM_THREADS; i++) {
        int *tid = malloc(sizeof(int));
        if (!tid) {
            fprintf(stderr, "Failed to allocate thread ID\n");
            cobalt_mutex_destroy(counter_mutex);
            return 1;
        }
        *tid = i;

        int rc = cobalt_thread_create(thread_func, tid, &threads[i]);
        if (rc != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            cobalt_mutex_destroy(counter_mutex);
            return 1;
        }
    }

    /* Join threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        cobalt_thread_join(threads[i]);
    }

    printf("\nFinal counter value: %d (expected: %d)\n", shared_counter, NUM_THREADS * ITERATIONS);

    if (shared_counter == NUM_THREADS * ITERATIONS) {
        cobalt_info("Concurrency test PASSED\n");
    } else {
        cobalt_error("Concurrency test FAILED\n");
    }

    /* Cleanup */
    cobalt_mutex_destroy(counter_mutex);

    printf("\n=== Demo complete ===\n");
    return 0;
}
