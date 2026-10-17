/**
 * @file benchmark_threadpool.c
 * @brief Benchmark suite for Cobalt work-stealing thread pool and task scheduler
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "cobalt/module/threadpool.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_TASKS 100000
#define BENCH_PARALLEL_FOR_ITEMS 1000000

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static void count_task(void *arg)
{
    atomic_int *counter = (atomic_int *)arg;
    atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
}

static void bench_throughput(size_t num_threads)
{
    cobalt_threadpool_t *pool = cobalt_threadpool_create(num_threads);
    if (!pool) {
        fprintf(stderr, "Failed to create thread pool\n");
        return;
    }

    atomic_int counter;
    atomic_init(&counter, 0);

    double start = current_time_ms();
    for (int i = 0; i < BENCH_TASKS; i++) {
        cobalt_threadpool_submit(pool, count_task, &counter);
    }
    cobalt_threadpool_wait_idle(pool);
    double elapsed = current_time_ms() - start;

    double tasks_sec = ((double)BENCH_TASKS / (elapsed / 1000.0));
    printf("ThreadPool throughput (%zu threads, %d tasks): %.2f ms (%.0f tasks/sec)\n",
           cobalt_threadpool_thread_count(pool),
           BENCH_TASKS,
           elapsed,
           tasks_sec);

    cobalt_threadpool_destroy(pool);
}

typedef struct {
    uint64_t *array;
} pfor_ctx_t;

static void pfor_kernel(size_t index, void *arg)
{
    pfor_ctx_t *ctx   = (pfor_ctx_t *)arg;
    ctx->array[index] = ctx->array[index] * 1664525U + 1013904223U;
}

static void bench_parallel_for(size_t num_threads)
{
    uint64_t *arr = (uint64_t *)malloc(sizeof(uint64_t) * BENCH_PARALLEL_FOR_ITEMS);
    if (!arr) {
        return;
    }
    for (size_t i = 0; i < BENCH_PARALLEL_FOR_ITEMS; i++) {
        arr[i] = (uint64_t)i;
    }

    /* Serial baseline */
    double serial_start = current_time_ms();
    for (size_t i = 0; i < BENCH_PARALLEL_FOR_ITEMS; i++) {
        arr[i] = arr[i] * 1664525U + 1013904223U;
    }
    double serial_ms = current_time_ms() - serial_start;

    /* Parallel-for */
    cobalt_threadpool_t *pool = cobalt_threadpool_create(num_threads);
    if (!pool) {
        free(arr);
        return;
    }

    pfor_ctx_t ctx       = {.array = arr};
    double     par_start = current_time_ms();
    cobalt_threadpool_parallel_for(pool, 0, BENCH_PARALLEL_FOR_ITEMS, 0, pfor_kernel, &ctx);
    double par_ms = current_time_ms() - par_start;

    double speedup = serial_ms / (par_ms > 0.001 ? par_ms : 0.001);
    printf("Parallel-for (%zu threads, %d items): Serial: %.2f ms | Parallel: %.2f ms | Speedup: "
           "%.2fx\n",
           cobalt_threadpool_thread_count(pool),
           BENCH_PARALLEL_FOR_ITEMS,
           serial_ms,
           par_ms,
           speedup);

    cobalt_threadpool_destroy(pool);
    free(arr);
}

int main(void)
{
    printf("============================================================\n");
    printf("Cobalt Thread Pool Benchmark Suite\n");
    printf("============================================================\n");

    bench_throughput(1);
    bench_throughput(4);
    bench_throughput(0); /* Auto-detect cores */

    bench_parallel_for(4);
    bench_parallel_for(0); /* Auto-detect cores */

    printf("============================================================\n");
    printf("Benchmark completed\n");
    return 0;
}
