/**
 * @file benchmark_lfskiplist.c
 * @brief Benchmark suite for Layer 4 Lock-Free SkipList with EBR
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "cobalt/container/lfskiplist.h"
#include "cobalt/container/treemap.h"
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

static int int_comparator(const void *a, const void *b)
{
    intptr_t ia = (intptr_t)a;
    intptr_t ib = (intptr_t)b;
    if (ia < ib) {
        return -1;
    }
    if (ia > ib) {
        return 1;
    }
    return 0;
}

typedef struct {
    cobalt_lfskiplist_t *sl;
    int                  thread_id;
    int                  num_ops;
    int                  read_pct;
    int                  key_range;
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
        int key = (seed >> 8) % w->key_range;

        if (op < w->read_pct) {
            void *val = NULL;
            cobalt_lfskiplist_get(w->sl, (void *)(intptr_t)key, &val);
        } else {
            cobalt_lfskiplist_put(w->sl, (void *)(intptr_t)key, (void *)(intptr_t)i, NULL);
        }
    }
    return NULL;
}

static void bench_read_heavy(void)
{
    printf("=== Lock-Free SkipList Read-Heavy Benchmark (90%% Read / 10%% Write) ===\n");
    cobalt_lfskiplist_t *sl = cobalt_lfskiplist_create(int_comparator);
    assert(sl != NULL);

    /* Pre-populate 10,000 elements */
    for (int i = 0; i < 10000; i++) {
        cobalt_lfskiplist_insert(sl, (void *)(intptr_t)i, (void *)(intptr_t)i);
    }

    const int num_threads    = 4;
    const int ops_per_thread = 50000;
    const int total_ops      = num_threads * ops_per_thread;

    cobalt_thread_t    threads[4];
    bench_worker_arg_t args[4];

    double start = current_time_ms();

    for (int i = 0; i < num_threads; i++) {
        args[i].sl        = sl;
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
    printf("  Throughput: %.2f ops/sec\n", throughput);
    printf("  Read-Heavy: PASSED\n\n");

    cobalt_lfskiplist_destroy(sl);
}

static void bench_balanced(void)
{
    printf("=== Lock-Free SkipList Balanced Benchmark (50%% Read / 50%% Write) ===\n");
    cobalt_lfskiplist_t *sl = cobalt_lfskiplist_create(int_comparator);
    assert(sl != NULL);

    for (int i = 0; i < 5000; i++) {
        cobalt_lfskiplist_insert(sl, (void *)(intptr_t)i, (void *)(intptr_t)i);
    }

    const int num_threads    = 4;
    const int ops_per_thread = 50000;
    const int total_ops      = num_threads * ops_per_thread;

    cobalt_thread_t    threads[4];
    bench_worker_arg_t args[4];

    double start = current_time_ms();

    for (int i = 0; i < num_threads; i++) {
        args[i].sl        = sl;
        args[i].thread_id = i;
        args[i].num_ops   = ops_per_thread;
        args[i].read_pct  = 50;
        args[i].key_range = 5000;
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
    printf("  Throughput: %.2f ops/sec\n", throughput);
    printf("  Balanced: PASSED\n\n");

    cobalt_lfskiplist_destroy(sl);
}

typedef struct {
    cobalt_treemap_t *map;
    cobalt_mutex_t   *lock;
    int               thread_id;
    int               num_ops;
    int               read_pct;
    int               key_range;
} mutex_bench_arg_t;

static char s_bench_keys[5000][16];

static void init_bench_keys(void)
{
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < 5000; i++) {
            snprintf(s_bench_keys[i], sizeof(s_bench_keys[i]), "k%04d", i);
        }
        initialized = true;
    }
}

static void *mutex_worker(void *arg)
{
    mutex_bench_arg_t *w    = (mutex_bench_arg_t *)arg;
    uint32_t           seed = (uint32_t)w->thread_id + 2026U;

    for (int i = 0; i < w->num_ops; i++) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;

        int op  = seed % 100;
        int key = (seed >> 8) % w->key_range;

        cobalt_mutex_lock(w->lock);
        if (op < w->read_pct) {
            void *val = cobalt_treemap_get(w->map, s_bench_keys[key]);
            (void)val;
        } else {
            cobalt_treemap_put(w->map, s_bench_keys[key], (void *)(intptr_t)i);
        }
        cobalt_mutex_unlock(w->lock);
    }
    return NULL;
}

static void *bench_string_worker(void *arg)
{
    bench_worker_arg_t *w    = (bench_worker_arg_t *)arg;
    uint32_t            seed = (uint32_t)w->thread_id + 2026U;

    for (int i = 0; i < w->num_ops; i++) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;

        int op  = seed % 100;
        int key = (seed >> 8) % w->key_range;

        if (op < w->read_pct) {
            void *val = NULL;
            cobalt_lfskiplist_get(w->sl, s_bench_keys[key], &val);
        } else {
            cobalt_lfskiplist_put(w->sl, s_bench_keys[key], (void *)(intptr_t)i, NULL);
        }
    }
    return NULL;
}

static void bench_lockfree_vs_mutex(void)
{
    printf("=== Lock-Free SkipList vs Mutex-Locked TreeMap Comparison ===\n");
    init_bench_keys();

    const int num_threads    = 4;
    const int ops_per_thread = 50000;
    const int total_ops      = num_threads * ops_per_thread;

    /* 1. Measure Mutex-Locked TreeMap */
    cobalt_treemap_t *tm   = cobalt_treemap_create();
    cobalt_mutex_t   *lock = cobalt_mutex_create();
    for (int i = 0; i < 5000; i++) {
        cobalt_treemap_put(tm, s_bench_keys[i], (void *)(intptr_t)i);
    }

    cobalt_thread_t   threads[4];
    mutex_bench_arg_t m_args[4];

    double start = current_time_ms();
    for (int i = 0; i < num_threads; i++) {
        m_args[i].map       = tm;
        m_args[i].lock      = lock;
        m_args[i].thread_id = i;
        m_args[i].num_ops   = ops_per_thread;
        m_args[i].read_pct  = 80;
        m_args[i].key_range = 5000;
        int trc             = cobalt_thread_create(mutex_worker, &m_args[i], &threads[i]);
        (void)trc;
        assert(trc == 0);
    }
    for (int i = 0; i < num_threads; i++) {
        cobalt_thread_join(threads[i]);
    }
    double mutex_elapsed = current_time_ms() - start;
    double mutex_tput    = (double)total_ops / (mutex_elapsed / 1000.0);
    printf("  [Mutex TreeMap]     Throughput: %.2f ops/sec (%.2f ms)\n", mutex_tput, mutex_elapsed);

    cobalt_mutex_destroy(lock);
    cobalt_treemap_destroy(tm);

    /* 2. Measure Lock-Free SkipList */
    cobalt_lfskiplist_t *sl = cobalt_lfskiplist_create((compare_func_t)strcmp);
    for (int i = 0; i < 5000; i++) {
        cobalt_lfskiplist_insert(sl, s_bench_keys[i], (void *)(intptr_t)i);
    }

    bench_worker_arg_t sl_args[4];
    start = current_time_ms();
    for (int i = 0; i < num_threads; i++) {
        sl_args[i].sl        = sl;
        sl_args[i].thread_id = i;
        sl_args[i].num_ops   = ops_per_thread;
        sl_args[i].read_pct  = 80;
        sl_args[i].key_range = 5000;
        int trc              = cobalt_thread_create(bench_string_worker, &sl_args[i], &threads[i]);
        (void)trc;
        assert(trc == 0);
    }
    for (int i = 0; i < num_threads; i++) {
        cobalt_thread_join(threads[i]);
    }
    double lf_elapsed = current_time_ms() - start;
    double lf_tput    = (double)total_ops / (lf_elapsed / 1000.0);
    double speedup    = lf_tput / mutex_tput;

    printf("  [Lock-Free SkipList] Throughput: %.2f ops/sec (%.2f ms)\n", lf_tput, lf_elapsed);
    printf("  => Speedup: %.2fx faster than Mutex TreeMap\n", speedup);
    printf("  Comparison: PASSED\n\n");

    cobalt_lfskiplist_destroy(sl);
}

int main(void)
{
    printf("====================================================\n");
    printf("Cobalt Lock-Free SkipList & EBR Benchmark Suite\n");
    printf("====================================================\n\n");

    bench_read_heavy();
    bench_balanced();
    bench_lockfree_vs_mutex();

    printf("Benchmark completed\n");
    return 0;
}
