#include "cobalt/container/hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main(void)
{
    printf("HashMap Benchmark\n");
    printf("=================\n");

    /* Test 1: Insert 10K strings */
    cobalt_hashmap_t *map = cobalt_hashmap_create(1024);
    char              key[64];
    int               value = 42;

    double start = current_time_ms();
    for (int i = 0; i < 10000; i++) {
        snprintf(key, 64, "key_%d", i);
        cobalt_hashmap_put(map, key, &value);
    }
    double insert_time = current_time_ms() - start;
    printf("Insert 10K strings: %.2f ms\n", insert_time);

    /* Test 2: Get 10K strings */
    start = current_time_ms();
    for (int i = 0; i < 10000; i++) {
        snprintf(key, 64, "key_%d", i);
        cobalt_hashmap_get(map, key);
    }
    double get_time = current_time_ms() - start;
    printf("Get 10K strings: %.2f ms\n", get_time);

    cobalt_hashmap_destroy(map);

    printf("Benchmark completed\n");
    return 0;
}
