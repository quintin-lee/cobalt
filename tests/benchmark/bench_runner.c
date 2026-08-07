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

int main(void)
{
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
