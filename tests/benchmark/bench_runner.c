/**
 * @file bench_runner.c
 * @brief Unified benchmark runner — executes all benchmarks with statistics
 */

#include "cobalt/container/deque.h"
#include "cobalt/container/hashmap.h"
#include "cobalt/container/list.h"
#include "cobalt/container/queue.h"
#include "cobalt/container/set.h"
#include "cobalt/container/stack.h"
#include "cobalt/container/treemap.h"
#include "cobalt/container/vector.h"
#include "cobalt/utils/benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BENCH_ITERS 100000

/* ===== Vector ===== */
static void bench_vector_push(void)
{
    cobalt_vector_t *v   = cobalt_vector_create(64);
    int              val = 42;
    for (int i = 0; i < BENCH_ITERS; i++) {
        cobalt_vector_push(v, &val);
    }
    cobalt_vector_destroy(v);
}

static void bench_vector_get(void)
{
    cobalt_vector_t *v   = cobalt_vector_create(64);
    int              val = 42;
    for (int i = 0; i < BENCH_ITERS; i++) {
        cobalt_vector_push(v, &val);
    }
    for (int i = 0; i < BENCH_ITERS; i++) {
        cobalt_vector_get(v, i);
    }
    cobalt_vector_destroy(v);
}

/* ===== HashMap ===== */
static void bench_hashmap_put(void)
{
    cobalt_hashmap_t *map = cobalt_hashmap_create(256);
    char              key[32];
    int               val = 42;
    for (int i = 0; i < BENCH_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_hashmap_put(map, key, &val);
    }
    cobalt_hashmap_destroy(map);
}

static void bench_hashmap_get(void)
{
    cobalt_hashmap_t *map = cobalt_hashmap_create(256);
    char              key[32];
    int               val = 42;
    for (int i = 0; i < BENCH_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_hashmap_put(map, key, &val);
    }
    for (int i = 0; i < BENCH_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_hashmap_get(map, key);
    }
    cobalt_hashmap_destroy(map);
}

/* ===== TreeMap ===== */
static void bench_treemap_put(void)
{
    cobalt_treemap_t *map = cobalt_treemap_create();
    char              key[32];
    int               val = 42;
    for (int i = 0; i < BENCH_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_treemap_put(map, key, &val);
    }
    cobalt_treemap_destroy(map);
}

static void bench_treemap_get(void)
{
    cobalt_treemap_t *map = cobalt_treemap_create();
    char              key[32];
    int               val = 42;
    for (int i = 0; i < BENCH_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_treemap_put(map, key, &val);
    }
    for (int i = 0; i < BENCH_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_treemap_get(map, key);
    }
    cobalt_treemap_destroy(map);
}

/* ===== List ===== */
static void bench_list_push(void)
{
    cobalt_list_t *list = cobalt_list_create();
    int            val  = 42;
    for (int i = 0; i < BENCH_ITERS; i++) {
        cobalt_list_push_back(list, &val);
    }
    cobalt_list_destroy(list);
}

/* ===== Deque ===== */
static void bench_deque_push(void)
{
    cobalt_deque_t *dq  = cobalt_deque_create();
    int             val = 42;
    for (int i = 0; i < BENCH_ITERS; i++) {
        cobalt_deque_push_front(dq, &val);
    }
    cobalt_deque_destroy(dq);
}

/* ===== Stack ===== */
static void bench_stack_push(void)
{
    cobalt_stack_t *st  = cobalt_stack_create();
    int             val = 42;
    for (int i = 0; i < BENCH_ITERS; i++) {
        cobalt_stack_push(st, &val);
    }
    cobalt_stack_destroy(st);
}

/* ===== Queue ===== */
static void bench_queue_enqueue(void)
{
    cobalt_queue_t *q   = cobalt_queue_create();
    int             val = 42;
    for (int i = 0; i < BENCH_ITERS; i++) {
        cobalt_queue_enqueue(q, &val);
    }
    cobalt_queue_destroy(q);
}

/* ===== Set ===== */
static void bench_set_insert(void)
{
    cobalt_set_t *set = cobalt_set_create(64);
    int           val = 42;
    for (int i = 0; i < BENCH_ITERS; i++) {
        cobalt_set_insert(set, &val);
    }
    cobalt_set_destroy(set);
}

int main(int argc, char *argv[])
{
    int json_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = 1;
        }
    }

    if (json_mode) {
        cobalt_bench_result_t results[16];
        memset(results, 0, sizeof(results));

        results[0].name = "vector_push";
        cobalt_bench_run("vector_push", 3, bench_vector_push, &results[0]);
        results[1].name = "vector_get";
        cobalt_bench_run("vector_get", 3, bench_vector_get, &results[1]);
        results[2].name = "hashmap_put";
        cobalt_bench_run("hashmap_put", 3, bench_hashmap_put, &results[2]);
        results[3].name = "hashmap_get";
        cobalt_bench_run("hashmap_get", 3, bench_hashmap_get, &results[3]);
        results[4].name = "treemap_put";
        cobalt_bench_run("treemap_put", 3, bench_treemap_put, &results[4]);
        results[5].name = "treemap_get";
        cobalt_bench_run("treemap_get", 3, bench_treemap_get, &results[5]);
        results[6].name = "list_push_back";
        cobalt_bench_run("list_push_back", 3, bench_list_push, &results[6]);
        results[7].name = "deque_push_front";
        cobalt_bench_run("deque_push_front", 3, bench_deque_push, &results[7]);
        results[8].name = "stack_push";
        cobalt_bench_run("stack_push", 3, bench_stack_push, &results[8]);
        results[9].name = "queue_enqueue";
        cobalt_bench_run("queue_enqueue", 3, bench_queue_enqueue, &results[9]);
        results[10].name = "set_insert";
        cobalt_bench_run("set_insert", 3, bench_set_insert, &results[10]);
        results[11].name = NULL;

        cobalt_bench_output_json(results, stdout);
        return 0;
    }

    printf("Cobalt Benchmark Suite\n");
    printf("======================\n\n");

    printf("--- Vector ---\n");
    BENCH_RUN("vector push (100k)", 3, bench_vector_push);
    BENCH_RUN("vector get (100k)", 3, bench_vector_get);

    printf("--- HashMap ---\n");
    BENCH_RUN("hashmap put (100k)", 3, bench_hashmap_put);
    BENCH_RUN("hashmap get (100k)", 3, bench_hashmap_get);

    printf("--- TreeMap ---\n");
    BENCH_RUN("treemap put (100k)", 3, bench_treemap_put);
    BENCH_RUN("treemap get (100k)", 3, bench_treemap_get);

    printf("--- List ---\n");
    BENCH_RUN("list push_back (100k)", 3, bench_list_push);

    printf("--- Deque ---\n");
    BENCH_RUN("deque push_front (100k)", 3, bench_deque_push);

    printf("--- Stack ---\n");
    BENCH_RUN("stack push (100k)", 3, bench_stack_push);

    printf("--- Queue ---\n");
    BENCH_RUN("queue enqueue (100k)", 3, bench_queue_enqueue);

    printf("--- Set ---\n");
    BENCH_RUN("set insert (100k)", 3, bench_set_insert);

    printf("\nBenchmark suite complete.\n");
    return 0;
}
