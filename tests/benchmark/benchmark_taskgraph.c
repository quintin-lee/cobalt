/**
 * @file benchmark_taskgraph.c
 * @brief Benchmark suite for Cobalt event-driven DAG TaskGraph scheduler
 */

#define _POSIX_C_SOURCE 199309L

#include "cobalt/module/taskgraph.h"
#include "cobalt/module/threadpool.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FANOUT_TASKS 10000
#define PIPELINE_LAYERS 500
#define PIPELINE_WIDTH 20 /* 500 * 20 = 10,000 tasks */

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static void bench_task_increment(void *arg)
{
    atomic_size_t *counter = (atomic_size_t *)arg;
    atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
}

static void bench_fanout_dag(cobalt_threadpool_t *pool)
{
    printf("1. Fan-out / Fan-in DAG (1 Root -> %d Workers -> 1 Sink):\n", FANOUT_TASKS);

    cobalt_taskgraph_t *g = cobalt_taskgraph_create();
    if (!g) {
        fprintf(stderr, "Failed to create taskgraph\n");
        return;
    }

    atomic_size_t counter;
    atomic_init(&counter, 0);

    cobalt_task_id_t root = cobalt_taskgraph_add_node(g, bench_task_increment, &counter);
    cobalt_task_id_t sink = cobalt_taskgraph_add_node(g, bench_task_increment, &counter);

    for (int i = 0; i < FANOUT_TASKS; i++) {
        cobalt_task_id_t worker = cobalt_taskgraph_add_node(g, bench_task_increment, &counter);
        cobalt_taskgraph_add_edge(g, root, worker);
        cobalt_taskgraph_add_edge(g, worker, sink);
    }

    double t_compile_0 = current_time_ms();
    if (cobalt_taskgraph_compile(g) != 0) {
        fprintf(stderr, "TaskGraph compilation failed!\n");
        cobalt_taskgraph_destroy(g);
        return;
    }
    double compile_ms = current_time_ms() - t_compile_0;

    size_t total_nodes = cobalt_taskgraph_node_count(g);
    double t_run_0     = current_time_ms();
    if (cobalt_taskgraph_run_and_wait(g, pool) != 0) {
        fprintf(stderr, "TaskGraph execution failed!\n");
        cobalt_taskgraph_destroy(g);
        return;
    }
    double run_ms = current_time_ms() - t_run_0;

    size_t executed = atomic_load(&counter);
    if (executed != total_nodes) {
        fprintf(stderr, "Validation failed: executed %zu != %zu\n", executed, total_nodes);
    }

    double throughput = (run_ms > 0.0) ? ((double)total_nodes / (run_ms / 1000.0)) : 0.0;
    printf(
        "   Compile: %.2f ms | Execution: %.2f ms (%zu tasks)\n", compile_ms, run_ms, total_nodes);
    printf("   Throughput: %.0f DAG tasks/sec\n\n", throughput);

    /* Test re-run without re-compiling */
    atomic_store(&counter, 0);
    double t_rerun_0 = current_time_ms();
    cobalt_taskgraph_run_and_wait(g, pool);
    double rerun_ms = current_time_ms() - t_rerun_0;
    printf("   Re-run (reuse graph): %.2f ms (%.0f DAG tasks/sec)\n\n",
           rerun_ms,
           (rerun_ms > 0.0) ? ((double)total_nodes / (rerun_ms / 1000.0)) : 0.0);

    cobalt_taskgraph_destroy(g);
}

static void bench_pipeline_dag(cobalt_threadpool_t *pool)
{
    printf("2. Layered Pipeline DAG (%d layers x %d width = %d tasks):\n",
           PIPELINE_LAYERS,
           PIPELINE_WIDTH,
           PIPELINE_LAYERS * PIPELINE_WIDTH);

    cobalt_taskgraph_t *g = cobalt_taskgraph_create();
    if (!g) {
        return;
    }

    atomic_size_t counter;
    atomic_init(&counter, 0);

    cobalt_task_id_t prev_layer[PIPELINE_WIDTH];
    cobalt_task_id_t curr_layer[PIPELINE_WIDTH];

    /* Layer 0 */
    for (int w = 0; w < PIPELINE_WIDTH; w++) {
        prev_layer[w] = cobalt_taskgraph_add_node(g, bench_task_increment, &counter);
    }

    /* Subsequent layers */
    for (int l = 1; l < PIPELINE_LAYERS; l++) {
        for (int w = 0; w < PIPELINE_WIDTH; w++) {
            curr_layer[w] = cobalt_taskgraph_add_node(g, bench_task_increment, &counter);
            /* Connect each node in curr layer to 2 nodes in prev layer (or all in prev layer) */
            cobalt_taskgraph_add_edge(g, prev_layer[w], curr_layer[w]);
            cobalt_taskgraph_add_edge(g, prev_layer[(w + 1) % PIPELINE_WIDTH], curr_layer[w]);
        }
        for (int w = 0; w < PIPELINE_WIDTH; w++) {
            prev_layer[w] = curr_layer[w];
        }
    }

    double t_compile_0 = current_time_ms();
    if (cobalt_taskgraph_compile(g) != 0) {
        fprintf(stderr, "Pipeline compilation failed!\n");
        cobalt_taskgraph_destroy(g);
        return;
    }
    double compile_ms = current_time_ms() - t_compile_0;

    size_t total_nodes = cobalt_taskgraph_node_count(g);
    double t_run_0     = current_time_ms();
    if (cobalt_taskgraph_run_and_wait(g, pool) != 0) {
        fprintf(stderr, "Pipeline execution failed!\n");
        cobalt_taskgraph_destroy(g);
        return;
    }
    double run_ms = current_time_ms() - t_run_0;

    double throughput = (run_ms > 0.0) ? ((double)total_nodes / (run_ms / 1000.0)) : 0.0;
    printf(
        "   Compile: %.2f ms | Execution: %.2f ms (%zu tasks)\n", compile_ms, run_ms, total_nodes);
    printf("   Throughput: %.0f DAG tasks/sec\n\n", throughput);

    cobalt_taskgraph_destroy(g);
}

int main(void)
{
    printf("=================================================================\n");
    printf("Cobalt TaskGraph DAG Scheduler Benchmark\n");
    printf("=================================================================\n\n");

    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    if (!pool) {
        fprintf(stderr, "Failed to create thread pool\n");
        return 1;
    }

    bench_fanout_dag(pool);
    bench_pipeline_dag(pool);

    cobalt_threadpool_destroy(pool);

    printf("Benchmark completed\n");
    return 0;
}
