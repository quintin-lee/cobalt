#include "cobalt/container/treemap.h"
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
    printf("TreeMap Benchmark\n");
    printf("==================\n");

    cobalt_treemap_t *map = cobalt_treemap_create();
    if (!map) {
        fprintf(stderr, "Failed to create treemap\n");
        return 1;
    }

    /* Test 1: Insert 10K strings */
    char   key[64];
    int   *values = malloc(sizeof(int) * 10000);
    if (!values) {
        fprintf(stderr, "Failed to allocate values\n");
        return 1;
    }
    double start = current_time_ms();
    for (int i = 0; i < 10000; i++) {
        snprintf(key, 64, "key_%d", i);
        values[i] = i;
        cobalt_treemap_put(map, key, &values[i]);
    }
    double insert_time = current_time_ms() - start;
    printf("Insert 10K strings: %.2f ms (size=%zu)\n", insert_time, cobalt_treemap_size(map));

    /* Test 2: Get 10K strings */
    start = current_time_ms();
    int hits = 0;
    for (int i = 0; i < 10000; i++) {
        snprintf(key, 64, "key_%d", i);
        if (cobalt_treemap_get(map, key) != NULL) {
            hits++;
        }
    }
    double get_time = current_time_ms() - start;
    printf("Get 10K strings: %.2f ms (hits=%d)\n", get_time, hits);

    /* Test 3: Remove 5K strings */
    start = current_time_ms();
    for (int i = 0; i < 5000; i++) {
        snprintf(key, 64, "key_%d", i);
        cobalt_treemap_remove(map, key);
    }
    double remove_time = current_time_ms() - start;
    printf("Remove 5K strings: %.2f ms (size=%zu)\n", remove_time, cobalt_treemap_size(map));

    cobalt_treemap_destroy(map);
    free(values);

    printf("Benchmark completed\n");
    return 0;
}
