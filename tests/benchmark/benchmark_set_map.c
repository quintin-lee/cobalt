#include "cobalt/container/set.h"
#include "cobalt/interface/map.h"
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
    printf("Set Map Interface Benchmark\n");
    printf("============================\n");

    cobalt_set_t *set = cobalt_set_create(1024);
    if (!set) {
        fprintf(stderr, "Failed to create set\n");
        return 1;
    }
    cobalt_map_t *map = (cobalt_map_t *)set;

    char key[64];

    /* Insert 10K */
    double start = current_time_ms();
    for (int i = 0; i < 10000; i++) {
        snprintf(key, 64, "key_%d", i);
        cobalt_map_put(map, key, strlen(key) + 1, (void *)1);
    }
    double insert_time = current_time_ms() - start;
    printf("Insert 10K:     %.2f ms (size=%zu)\n", insert_time, cobalt_map_size(map));

    /* Contains 10K */
    start = current_time_ms();
    int hits = 0;
    for (int i = 0; i < 10000; i++) {
        snprintf(key, 64, "key_%d", i);
        if (cobalt_map_contains(map, key, strlen(key) + 1)) {
            hits++;
        }
    }
    double contains_time = current_time_ms() - start;
    printf("Contains 10K:   %.2f ms (hits=%d)\n", contains_time, hits);

    /* Remove 5K */
    start = current_time_ms();
    for (int i = 0; i < 5000; i++) {
        snprintf(key, 64, "key_%d", i);
        cobalt_map_remove(map, key, strlen(key) + 1);
    }
    double remove_time = current_time_ms() - start;
    printf("Remove 5K:      %.2f ms (size=%zu)\n", remove_time, cobalt_map_size(map));

    /* Iterate */
    start = current_time_ms();
    int count = 0;
    cobalt_map_iterator_t *iter = cobalt_map_iterator_create(map);
    if (iter) {
        while (cobalt_map_iterator_has_next(iter)) {
            cobalt_map_iterator_next(iter);
            count++;
        }
        cobalt_map_iterator_destroy(iter);
    }
    double iter_time = current_time_ms() - start;
    printf("Iterate:        %.2f ms (yielded=%d)\n", iter_time, count);

    /* Clear */
    start = current_time_ms();
    cobalt_map_clear(map);
    double clear_time = current_time_ms() - start;
    printf("Clear:          %.2f ms (size=%zu)\n", clear_time, cobalt_map_size(map));

    map->destroy(map);

    printf("Benchmark completed\n");
    return 0;
}
