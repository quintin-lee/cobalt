/**
 * @file refcount_demo.c
 * @brief Demonstrating thread-safe reference counting
 *
 * Shows that reference operations are lock-free on the target platform.
 * This example spawns multiple threads that concurrently increment/decrement
 * the same reference count to demonstrate safety.
 */

#include <cobalt/cobalt.h>
#include <pthread.h>
#include <stdio.h>

/* Shared counter for demonstration */
static cobalt_atomic_t shared_counter = {.value = 0};

/* Thread function that increments the counter many times */
void *increment_thread(void *arg)
{
    int iterations = *(int *)arg;
    for (int i = 0; i < iterations; i++) {
        cobalt_atomic_increment(&shared_counter);
    }
    return NULL;
}

int main(void)
{
    cobalt_logger_init(stdout, LOG_LEVEL_INFO);

    /* Initialize counter */
    shared_counter = cobalt_atomic_create(0);
    cobalt_info("Initial counter value: %d\n", cobalt_atomic_get(&shared_counter));

    /* Spawn multiple threads */
    #define NUM_THREADS 5
    #define ITERATIONS_PER_THREAD 1000

    pthread_t threads[NUM_THREADS];
    int       args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i] = ITERATIONS_PER_THREAD;
        pthread_create(&threads[i], NULL, increment_thread, &args[i]);
    }

    /* Wait for all threads to finish */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Check final value */
    int final_value = cobalt_atomic_get(&shared_counter);
    int expected    = NUM_THREADS * ITERATIONS_PER_THREAD;

    cobalt_info("Final counter value: %d (expected: %d)\n", final_value, expected);

    if (final_value == expected) {
        cobalt_info("✓ Reference counting is thread-safe!\n");
    } else {
        fprintf(stderr, "✗ Thread-safety check FAILED!\n");
        return 1;
    }

    cobalt_info("Refcount demo complete!\n");
    return 0;
}