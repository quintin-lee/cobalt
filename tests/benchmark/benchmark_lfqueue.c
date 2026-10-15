/**
 * @file benchmark_lfqueue.c
 * @brief Benchmark comparing lock-free MPMC queue vs mutex-based thread-safe queue
 */

#include "cobalt/container/lfqueue.h"
#include "cobalt/container/threadsafewrapper.h"
#include "cobalt/platform/thread.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_SINGLE_OPS 200000
#define BENCH_CONCURRENT_OPS 50000
#define BENCH_THREADS 4

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* ===== Single Threaded Benchmark ===== */

static void bench_lfqueue_single(void)
{
    cobalt_lfqueue_t *q     = cobalt_lfqueue_create(1024);
    void             *dummy = (void *)0x1234;
    void             *out   = NULL;

    double start = current_time_ms();
    for (int i = 0; i < BENCH_SINGLE_OPS; i++) {
        cobalt_lfqueue_enqueue(q, dummy);
        cobalt_lfqueue_dequeue(q, &out);
    }
    double elapsed = current_time_ms() - start;
    double ops_sec = (BENCH_SINGLE_OPS * 2.0) / (elapsed / 1000.0);
    (void)out;

    printf("Single-thread LFQueue: %.2f ms (%.0f ops/sec)\n", elapsed, ops_sec);
    cobalt_lfqueue_destroy(q);
}

static void bench_tsqueue_single(void)
{
    cobalt_tsqueue_t *q     = cobalt_tsqueue_create();
    void             *dummy = (void *)0x1234;
    void             *out   = NULL;

    double start = current_time_ms();
    for (int i = 0; i < BENCH_SINGLE_OPS; i++) {
        cobalt_tsqueue_enqueue(q, dummy);
        out = cobalt_tsqueue_dequeue(q);
    }
    double elapsed = current_time_ms() - start;
    double ops_sec = (BENCH_SINGLE_OPS * 2.0) / (elapsed / 1000.0);
    (void)out;

    printf("Single-thread TSQueue: %.2f ms (%.0f ops/sec)\n", elapsed, ops_sec);
    cobalt_tsqueue_destroy(q);
}

/* ===== Multi Threaded Concurrent Benchmark ===== */

typedef struct {
    cobalt_lfqueue_t *lfq;
    cobalt_tsqueue_t *tsq;
    int               thread_id;
} thread_bench_arg_t;

static void *lf_bench_thread(void *arg)
{
    thread_bench_arg_t *a = (thread_bench_arg_t *)arg;
    void               *out;
    for (int i = 0; i < BENCH_CONCURRENT_OPS; i++) {
        while (cobalt_lfqueue_enqueue(a->lfq, (void *)(uintptr_t)(i + 1)) != 0) {
            cobalt_thread_yield();
        }
        while (cobalt_lfqueue_dequeue(a->lfq, &out) != 0) {
            cobalt_thread_yield();
        }
    }
    (void)out;
    return NULL;
}

static void *ts_bench_thread(void *arg)
{
    thread_bench_arg_t *a = (thread_bench_arg_t *)arg;
    void               *out;
    for (int i = 0; i < BENCH_CONCURRENT_OPS; i++) {
        cobalt_tsqueue_enqueue(a->tsq, (void *)(uintptr_t)(i + 1));
        out = cobalt_tsqueue_dequeue(a->tsq);
    }
    (void)out;
    return NULL;
}

static void bench_concurrent_comparison(void)
{
    cobalt_thread_t    threads[BENCH_THREADS];
    thread_bench_arg_t args[BENCH_THREADS];

    /* Test TSQueue */
    cobalt_tsqueue_t *tsq = cobalt_tsqueue_create();
    for (int i = 0; i < BENCH_THREADS; i++) {
        args[i].tsq       = tsq;
        args[i].thread_id = i;
    }
    double start_ts = current_time_ms();
    for (int i = 0; i < BENCH_THREADS; i++) {
        cobalt_thread_create(ts_bench_thread, &args[i], &threads[i]);
    }
    for (int i = 0; i < BENCH_THREADS; i++) {
        cobalt_thread_join(threads[i]);
    }
    double elapsed_ts = current_time_ms() - start_ts;
    double ts_ops_sec = (BENCH_THREADS * BENCH_CONCURRENT_OPS * 2.0) / (elapsed_ts / 1000.0);
    printf("Multi-thread (4 threads) TSQueue (Mutex): %.2f ms (%.0f ops/sec)\n",
           elapsed_ts,
           ts_ops_sec);
    cobalt_tsqueue_destroy(tsq);

    /* Test LFQueue */
    cobalt_lfqueue_t *lfq = cobalt_lfqueue_create(1024);
    for (int i = 0; i < BENCH_THREADS; i++) {
        args[i].lfq       = lfq;
        args[i].thread_id = i;
    }
    double start_lf = current_time_ms();
    for (int i = 0; i < BENCH_THREADS; i++) {
        cobalt_thread_create(lf_bench_thread, &args[i], &threads[i]);
    }
    for (int i = 0; i < BENCH_THREADS; i++) {
        cobalt_thread_join(threads[i]);
    }
    double elapsed_lf = current_time_ms() - start_lf;
    double lf_ops_sec = (BENCH_THREADS * BENCH_CONCURRENT_OPS * 2.0) / (elapsed_lf / 1000.0);
    printf("Multi-thread (4 threads) LFQueue (Lock-Free): %.2f ms (%.0f ops/sec)\n",
           elapsed_lf,
           lf_ops_sec);
    cobalt_lfqueue_destroy(lfq);

    if (elapsed_lf > 0) {
        printf("Speedup factor: %.2fx\n", elapsed_ts / elapsed_lf);
    }
}

int main(void)
{
    printf("LFQueue vs TSQueue Benchmark\n");
    printf("============================\n");

    bench_tsqueue_single();
    bench_lfqueue_single();
    printf("\n");
    bench_concurrent_comparison();

    printf("\nBenchmark completed\n");
    return 0;
}
