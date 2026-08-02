#include "cobalt/container/set.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main(void)
{
    printf("Set Benchmark\n");
    printf("=============\n");

    cobalt_set_t *set = cobalt_set_create(1024);
    if (!set) {
        fprintf(stderr, "Failed to create set\n");
        return 1;
    }

    /* Test 1: Insert 10K strings */
    char   key[64];
    double start = current_time_ms();
    for (int i = 0; i < 10000; i++) {
        snprintf(key, 64, "item_%d", i);
        cobalt_set_insert(set, key);
    }
    double insert_time = current_time_ms() - start;
    printf("Insert 10K strings: %.2f ms (size=%zu)\n", insert_time, cobalt_set_size(set));

    /* Test 2: Contains 10K lookups */
    start    = current_time_ms();
    int hits = 0;
    for (int i = 0; i < 10000; i++) {
        snprintf(key, 64, "item_%d", i);
        if (cobalt_set_contains(set, key)) {
            hits++;
        }
    }
    double contains_time = current_time_ms() - start;
    printf("Contains 10K strings: %.2f ms (hits=%d)\n", contains_time, hits);

    /* Test 3: Remove 10K strings */
    start = current_time_ms();
    for (int i = 0; i < 10000; i++) {
        snprintf(key, 64, "item_%d", i);
        cobalt_set_remove(set, key);
    }
    double remove_time = current_time_ms() - start;
    printf("Remove 10K strings: %.2f ms (size=%zu)\n", remove_time, cobalt_set_size(set));

    cobalt_set_destroy(set);

    printf("Benchmark completed\n");
    return 0;
}
