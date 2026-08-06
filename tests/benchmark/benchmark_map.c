#include "cobalt/container/hashmap.h"
#include "cobalt/container/treemap.h"
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

static void run_benchmark(cobalt_map_t *map, const char *name)
{
    printf("\n--- %s (via cobalt_map_t interface) ---\n", name);

    char   key[64];
    int   *values = malloc(sizeof(int) * 10000);
    if (!values) {
        fprintf(stderr, "Failed to allocate values\n");
        return;
    }

    /* Insert 10K entries */
    double start = current_time_ms();
    for (int i = 0; i < 10000; i++) {
        snprintf(key, 64, "key_%d", i);
        values[i] = i;
        cobalt_map_put(map, key, strlen(key) + 1, &values[i]);
    }
    double insert_time = current_time_ms() - start;
    printf("  Insert 10K: %.2f ms (size=%zu)\n", insert_time, cobalt_map_size(map));

    /* Get 10K entries */
    start = current_time_ms();
    int hits = 0;
    for (int i = 0; i < 10000; i++) {
        snprintf(key, 64, "key_%d", i);
        if (cobalt_map_get(map, key, strlen(key) + 1) != NULL) {
            hits++;
        }
    }
    double get_time = current_time_ms() - start;
    printf("  Get 10K:    %.2f ms (hits=%d)\n", get_time, hits);

    /* Remove 5K entries */
    start = current_time_ms();
    for (int i = 0; i < 5000; i++) {
        snprintf(key, 64, "key_%d", i);
        cobalt_map_remove(map, key, strlen(key) + 1);
    }
    double remove_time = current_time_ms() - start;
    printf("  Remove 5K:  %.2f ms (size=%zu)\n", remove_time, cobalt_map_size(map));

    /* Iterate all remaining */
    start = current_time_ms();
    int iter_count = 0;
    cobalt_map_iterator_t *iter = cobalt_map_iterator_create(map);
    if (iter) {
        while (cobalt_map_iterator_has_next(iter)) {
            cobalt_map_iterator_next(iter);
            iter_count++;
        }
        cobalt_map_iterator_destroy(iter);
    }
    double iter_time = current_time_ms() - start;
    printf("  Iterate:    %.2f ms (yielded=%d)\n", iter_time, iter_count);

    cobalt_map_t *destroy_map = map;
    destroy_map->destroy(destroy_map);
    free(values);
}

int main(void)
{
    printf("Map Interface Benchmark\n");
    printf("=======================\n");

    cobalt_hashmap_t *hm = cobalt_hashmap_create(1024);
    cobalt_treemap_t *tm = cobalt_treemap_create();
    if (!hm || !tm) {
        fprintf(stderr, "Failed to create maps\n");
        return 1;
    }

    run_benchmark((cobalt_map_t *)hm, "HashMap");
    run_benchmark((cobalt_map_t *)tm, "TreeMap");

    printf("\nBenchmark completed\n");
    return 0;
}
