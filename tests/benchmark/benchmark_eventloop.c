/**
 * @file benchmark_eventloop.c
 * @brief Benchmark suite for Cobalt event loop, timing wheel, and zero-copy buffer
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "cobalt/container/buffer.h"
#include "cobalt/module/eventloop.h"
#include "cobalt/platform/thread.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCH_TIMERS_COUNT 100000
#define BENCH_POST_TASKS 100000
#define BENCH_BUFFER_ITERS 500000
#define BENCH_BUFFER_CHUNK 1024

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static void dummy_timer_cb(uint64_t timer_id, void *user_data)
{
    (void)timer_id;
    (void)user_data;
}

static void bench_timing_wheel(void)
{
    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    if (!loop) {
        return;
    }

    uint64_t *timer_ids = (uint64_t *)malloc(sizeof(uint64_t) * BENCH_TIMERS_COUNT);
    if (!timer_ids) {
        cobalt_eventloop_destroy(loop);
        return;
    }

    /* 1. Measure 100,000 timer additions */
    double start_add = current_time_ms();
    for (int i = 0; i < BENCH_TIMERS_COUNT; i++) {
        uint64_t timeout = (uint64_t)((i % 5000) + 1);
        timer_ids[i]     = cobalt_eventloop_add_timer(loop, timeout, 0, dummy_timer_cb, NULL);
    }
    double elapsed_add = current_time_ms() - start_add;
    double add_ops_sec = ((double)BENCH_TIMERS_COUNT / (elapsed_add / 1000.0));

    /* 2. Measure 50,000 timer cancellations */
    double start_del = current_time_ms();
    for (int i = 0; i < BENCH_TIMERS_COUNT / 2; i++) {
        cobalt_eventloop_del_timer(loop, timer_ids[i * 2]);
    }
    double elapsed_del = current_time_ms() - start_del;
    double del_ops_sec = ((double)(BENCH_TIMERS_COUNT / 2) / (elapsed_del / 1000.0));

    printf("Timing Wheel Add (%d timers):    %.2f ms (%.0f ops/sec)\n",
           BENCH_TIMERS_COUNT,
           elapsed_add,
           add_ops_sec);
    printf("Timing Wheel Cancel (%d timers): %.2f ms (%.0f ops/sec)\n",
           BENCH_TIMERS_COUNT / 2,
           elapsed_del,
           del_ops_sec);

    free(timer_ids);
    cobalt_eventloop_destroy(loop);
}

typedef struct {
    cobalt_eventloop_t *loop;
    int                 count;
    atomic_int         *counter;
} bench_post_worker_t;

static void bench_post_cb(void *arg)
{
    atomic_int *counter = (atomic_int *)arg;
    atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
}

static void *bench_post_worker(void *arg)
{
    bench_post_worker_t *w = (bench_post_worker_t *)arg;
    for (int i = 0; i < w->count; i++) {
        while (cobalt_eventloop_post(w->loop, bench_post_cb, w->counter) != 0) {
            cobalt_thread_yield();
        }
    }
    return NULL;
}

static void bench_cross_thread_post(void)
{
    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    if (!loop) {
        return;
    }

    atomic_int counter;
    atomic_init(&counter, 0);

    const int           threads_count = 4;
    const int           per_thread    = BENCH_POST_TASKS / threads_count;
    cobalt_thread_t     threads[4];
    bench_post_worker_t workers[4];

    double start = current_time_ms();
    for (int i = 0; i < threads_count; i++) {
        workers[i].loop    = loop;
        workers[i].count   = per_thread;
        workers[i].counter = &counter;
        cobalt_thread_create(bench_post_worker, &workers[i], &threads[i]);
    }

    while (atomic_load_explicit(&counter, memory_order_relaxed) < BENCH_POST_TASKS) {
        cobalt_eventloop_iteration(loop);
    }

    for (int i = 0; i < threads_count; i++) {
        cobalt_thread_join(threads[i]);
    }
    double elapsed = current_time_ms() - start;

    double ops_sec = ((double)BENCH_POST_TASKS / (elapsed / 1000.0));
    printf("Cross-Thread Post (%d tasks, 4 threads): %.2f ms (%.0f tasks/sec)\n",
           BENCH_POST_TASKS,
           elapsed,
           ops_sec);

    cobalt_eventloop_destroy(loop);
}

static void bench_buffer_throughput(void)
{
    cobalt_buffer_t *buf = cobalt_buffer_create(65536);
    if (!buf) {
        return;
    }

    char chunk[BENCH_BUFFER_CHUNK];
    memset(chunk, 'A', sizeof(chunk));

    double start = current_time_ms();
    for (int i = 0; i < BENCH_BUFFER_ITERS; i++) {
        cobalt_buffer_append(buf, chunk, sizeof(chunk));
        cobalt_buffer_retrieve(buf, sizeof(chunk));
    }
    double elapsed = current_time_ms() - start;

    double total_bytes = (double)BENCH_BUFFER_ITERS * (double)sizeof(chunk);
    double mb_sec      = (total_bytes / (1024.0 * 1024.0)) / (elapsed / 1000.0);
    double gb_sec      = mb_sec / 1024.0;

    printf("Zero-Copy Buffer (%.0f MB streamed): %.2f ms (%.2f GB/sec)\n",
           total_bytes / (1024.0 * 1024.0),
           elapsed,
           gb_sec);

    cobalt_buffer_destroy(buf);
}

int main(void)
{
    printf("============================================================\n");
    printf("Cobalt EventLoop & Buffer Benchmark Suite\n");
    printf("============================================================\n");

    bench_timing_wheel();
    bench_cross_thread_post();
    bench_buffer_throughput();

    printf("============================================================\n");
    printf("Benchmark completed\n");
    return 0;
}
