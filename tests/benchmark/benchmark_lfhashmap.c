/**
 * @file benchmark_lfhashmap.c
 * @brief Benchmark suite for Layer 4 Lock-Free Concurrent Extensible Hashmap
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "cobalt/container/lfhashmap.h"
#include "cobalt/platform/thread.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

typedef struct {
    cobalt_lfhashmap_t *map;
    int                 thread_id;
    int                 num_ops;
    int                 read_pct;
    int                 key_range;
} bench_worker_arg_t;

static void *bench_worker(void *arg)
{
    bench_worker_arg_t *w    = (bench_worker_arg_t *)arg;
    uint32_t            seed = (uint32_t)w->thread_id + 1337U;

    for (int i = 0; i < w->num_ops; i++) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;

        int op  = seed % 100;
        int key = (int)((seed >> 8) % (uint32_t)w->key_range) + 1;

        if (op < w->read_pct) {
            void *val = NULL;
            cobalt_lfhashmap_get(w->map, (void *)(intptr_t)key, &val);
        } else {
            cobalt_lfhashmap_put(w->map, (void *)(intptr_t)key, (void *)(intptr_t)i, NULL);
        }
    }
    return NULL;
}

static void bench_read_heavy(void)
{
    printf("=== Lock-Free Hashmap Read-Heavy Benchmark (90%% Read / 10%% Write) ===\n");
    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create_ptr();
    assert(map != NULL);

    /* Pre-populate 10,000 elements */
    for (int i = 1; i <= 10000; i++) {
        cobalt_lfhashmap_insert(map, (void *)(intptr_t)i, (void *)(intptr_t)i);
    }

    const int num_threads    = 4;
    const int ops_per_thread = 50000;
    const int total_ops      = num_threads * ops_per_thread;

    cobalt_thread_t    threads[4];
    bench_worker_arg_t args[4];

    double start = current_time_ms();

    for (int i = 0; i < num_threads; i++) {
        args[i].map       = map;
        args[i].thread_id = i;
        args[i].num_ops   = ops_per_thread;
        args[i].read_pct  = 90;
        args[i].key_range = 10000;
        int trc           = cobalt_thread_create(bench_worker, &args[i], &threads[i]);
        (void)trc;
        assert(trc == 0);
    }

    for (int i = 0; i < num_threads; i++) {
        cobalt_thread_join(threads[i]);
    }

    double elapsed    = current_time_ms() - start;
    double throughput = (double)total_ops / (elapsed / 1000.0);

    printf("  Completed %d ops in %.2f ms\n", total_ops, elapsed);
    printf("  Throughput: %.2f ops/sec (%.2f M ops/sec)\n", throughput, throughput / 1000000.0);
    printf("  Read-Heavy: PASSED\n\n");

    cobalt_lfhashmap_destroy(map);
}

static void bench_write_heavy(void)
{
    printf("=== Lock-Free Hashmap Write-Heavy Benchmark (50%% Read / 50%% Write) ===\n");
    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create_ptr();
    assert(map != NULL);

    for (int i = 1; i <= 10000; i++) {
        cobalt_lfhashmap_insert(map, (void *)(intptr_t)i, (void *)(intptr_t)i);
    }

    const int num_threads    = 4;
    const int ops_per_thread = 50000;
    const int total_ops      = num_threads * ops_per_thread;

    cobalt_thread_t    threads[4];
    bench_worker_arg_t args[4];

    double start = current_time_ms();

    for (int i = 0; i < num_threads; i++) {
        args[i].map       = map;
        args[i].thread_id = i;
        args[i].num_ops   = ops_per_thread;
        args[i].read_pct  = 50;
        args[i].key_range = 10000;
        int trc           = cobalt_thread_create(bench_worker, &args[i], &threads[i]);
        (void)trc;
        assert(trc == 0);
    }

    for (int i = 0; i < num_threads; i++) {
        cobalt_thread_join(threads[i]);
    }

    double elapsed    = current_time_ms() - start;
    double throughput = (double)total_ops / (elapsed / 1000.0);

    printf("  Completed %d ops in %.2f ms\n", total_ops, elapsed);
    printf("  Throughput: %.2f ops/sec (%.2f M ops/sec)\n", throughput, throughput / 1000000.0);
    printf("  Write-Heavy: PASSED\n\n");

    cobalt_lfhashmap_destroy(map);
}

static void bench_concurrency_scaling(void)
{
    printf("=== Lock-Free Hashmap Concurrency Scaling Benchmark ===\n");
    int thread_counts[] = {1, 2, 4};
    int num_configs     = sizeof(thread_counts) / sizeof(thread_counts[0]);

    for (int tc = 0; tc < num_configs; tc++) {
        int tcount = thread_counts[tc];

        cobalt_lfhashmap_t *map = cobalt_lfhashmap_create_ptr();
        assert(map != NULL);

        for (int i = 1; i <= 10000; i++) {
            cobalt_lfhashmap_insert(map, (void *)(intptr_t)i, (void *)(intptr_t)i);
        }

        const int ops_per_thread = 50000;
        const int total_ops      = tcount * ops_per_thread;

        cobalt_thread_t    threads[4];
        bench_worker_arg_t args[4];

        double start = current_time_ms();

        for (int i = 0; i < tcount; i++) {
            args[i].map       = map;
            args[i].thread_id = i;
            args[i].num_ops   = ops_per_thread;
            args[i].read_pct  = 90;
            args[i].key_range = 10000;
            cobalt_thread_create(bench_worker, &args[i], &threads[i]);
        }

        for (int i = 0; i < tcount; i++) {
            cobalt_thread_join(threads[i]);
        }

        double elapsed    = current_time_ms() - start;
        double throughput = (double)total_ops / (elapsed / 1000.0);

        printf("  Threads: %d | Total Ops: %d | Time: %.2f ms | Throughput: %.2f M ops/sec\n",
               tcount,
               total_ops,
               elapsed,
               throughput / 1000000.0);

        cobalt_lfhashmap_destroy(map);
    }
    printf("  Concurrency Scaling: PASSED\n\n");
}

static void bench_resizing_latency(void)
{
    printf("=== Lock-Free Hashmap Dynamic Resizing Latency Benchmark ===\n");
    cobalt_lfhashmap_t *map = cobalt_lfhashmap_create_ptr();
    assert(map != NULL);

    const int total = 100000;
    double    start = current_time_ms();

    for (int i = 1; i <= total; i++) {
        cobalt_lfhashmap_insert(map, (void *)(intptr_t)i, (void *)(intptr_t)i);
    }

    double elapsed    = current_time_ms() - start;
    double throughput = (double)total / (elapsed / 1000.0);

    printf("  Inserted %d keys (capacity expanded to %zu buckets) in %.2f ms\n",
           total,
           cobalt_lfhashmap_bucket_count(map),
           elapsed);
    printf(
        "  Insert throughput: %.2f ops/sec (%.2f M ops/sec)\n", throughput, throughput / 1000000.0);
    printf("  Resizing Latency: PASSED\n\n");

    cobalt_lfhashmap_destroy(map);
}

int main(void)
{
    printf("Starting Cobalt Lock-Free Hashmap Benchmark Suite...\n\n");
    bench_read_heavy();
    bench_write_heavy();
    bench_concurrency_scaling();
    bench_resizing_latency();
    printf("All Lock-Free Hashmap benchmarks finished successfully!\n");
    return 0;
}
