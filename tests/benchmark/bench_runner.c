/**
 * @file bench_runner.c
 * @brief Unified benchmark runner — executes all benchmarks with statistics
 */

#include "cobalt/algorithm/sort.h"
#include "cobalt/container/deque.h"
#include "cobalt/container/hashmap.h"
#include "cobalt/container/lfqueue.h"
#include "cobalt/container/list.h"
#include "cobalt/container/queue.h"
#include "cobalt/container/set.h"
#include "cobalt/container/stack.h"
#include "cobalt/container/treemap.h"
#include "cobalt/container/vector.h"
#include "cobalt/module/json.h"
#include "cobalt/module/json_sax.h"
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

/* ===== LFQueue ===== */
static void bench_lfqueue(void)
{
    cobalt_lfqueue_t *q   = cobalt_lfqueue_create(1024);
    void             *val = (void *)42;
    void             *out = NULL;
    for (int i = 0; i < BENCH_ITERS; i++) {
        cobalt_lfqueue_enqueue(q, val);
        cobalt_lfqueue_dequeue(q, &out);
    }
    cobalt_lfqueue_destroy(q);
}

/* ===== JSON ===== */
static const char *g_bench_json_sample =
    "{\"name\":\"Cobalt\",\"version\":2.4,\"active\":true,\"features\":[\"sax\",\"arena\","
    "\"swar\"],\"data\":{\"count\":42,\"ratio\":3.14159}}";

static void bench_json_sax(void)
{
    cobalt_json_sax_handler_t h;
    memset(&h, 0, sizeof(h));
    size_t len = strlen(g_bench_json_sample);
    for (int i = 0; i < 20000; i++) {
        cobalt_json_sax_parse(g_bench_json_sample, len, &h, NULL, NULL);
    }
}

static void bench_json_dom(void)
{
    for (int i = 0; i < 20000; i++) {
        json_node_t *node = json_parse(g_bench_json_sample);
        json_destroy(node);
    }
}

/* ===== Sort ===== */
static int bench_sort_cmp_int(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

static int *g_bench_random_ints = NULL;
static int *g_bench_sorted_ints = NULL;

static void ensure_sort_bench_data(void)
{
    if (!g_bench_random_ints) {
        g_bench_random_ints = (int *)malloc(sizeof(int) * BENCH_ITERS);
        if (g_bench_random_ints) {
            uint32_t seed = 42;
            for (int i = 0; i < BENCH_ITERS; i++) {
                seed                   = seed * 1664525u + 1013904223u;
                g_bench_random_ints[i] = (int)(seed & 0x7FFFFFFF);
            }
        }
    }
    if (!g_bench_sorted_ints) {
        g_bench_sorted_ints = (int *)malloc(sizeof(int) * BENCH_ITERS);
        if (g_bench_sorted_ints) {
            for (int i = 0; i < BENCH_ITERS; i++) {
                g_bench_sorted_ints[i] = i;
            }
        }
    }
}

static void bench_qsort_random(void)
{
    ensure_sort_bench_data();
    int *arr = (int *)malloc(sizeof(int) * BENCH_ITERS);
    if (!arr || !g_bench_random_ints) {
        free(arr);
        return;
    }
    memcpy(arr, g_bench_random_ints, sizeof(int) * BENCH_ITERS);
    cobalt_qsort(arr, BENCH_ITERS, sizeof(int), bench_sort_cmp_int);
    free(arr);
}

static void bench_qsort_sorted(void)
{
    ensure_sort_bench_data();
    int *arr = (int *)malloc(sizeof(int) * BENCH_ITERS);
    if (!arr || !g_bench_sorted_ints) {
        free(arr);
        return;
    }
    memcpy(arr, g_bench_sorted_ints, sizeof(int) * BENCH_ITERS);
    cobalt_qsort(arr, BENCH_ITERS, sizeof(int), bench_sort_cmp_int);
    free(arr);
}

static void bench_stable_sort_random(void)
{
    ensure_sort_bench_data();
    int *arr = (int *)malloc(sizeof(int) * BENCH_ITERS);
    if (!arr || !g_bench_random_ints) {
        free(arr);
        return;
    }
    memcpy(arr, g_bench_random_ints, sizeof(int) * BENCH_ITERS);
    cobalt_stable_sort(arr, BENCH_ITERS, sizeof(int), bench_sort_cmp_int);
    free(arr);
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
        cobalt_bench_result_t results[20];
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
        results[11].name = "lfqueue";
        cobalt_bench_run("lfqueue", 3, bench_lfqueue, &results[11]);
        results[12].name = "json_sax";
        cobalt_bench_run("json_sax", 3, bench_json_sax, &results[12]);
        results[13].name = "json_dom";
        cobalt_bench_run("json_dom", 3, bench_json_dom, &results[13]);
        results[14].name = "qsort_random";
        cobalt_bench_run("qsort_random", 3, bench_qsort_random, &results[14]);
        results[15].name = "qsort_sorted";
        cobalt_bench_run("qsort_sorted", 3, bench_qsort_sorted, &results[15]);
        results[16].name = "stable_sort_random";
        cobalt_bench_run("stable_sort_random", 3, bench_stable_sort_random, &results[16]);
        results[17].name = NULL;

        cobalt_bench_output_json(results, stdout);
        free(g_bench_random_ints);
        free(g_bench_sorted_ints);
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

    printf("--- LFQueue ---\n");
    BENCH_RUN("lfqueue enq/deq (100k)", 3, bench_lfqueue);

    printf("--- Set ---\n");
    BENCH_RUN("set insert (100k)", 3, bench_set_insert);

    printf("--- JSON ---\n");
    BENCH_RUN("json sax parse (20k)", 3, bench_json_sax);
    BENCH_RUN("json dom parse+destroy (20k)", 3, bench_json_dom);

    printf("--- Sort ---\n");
    BENCH_RUN("qsort random (100k)", 3, bench_qsort_random);
    BENCH_RUN("qsort sorted (100k)", 3, bench_qsort_sorted);
    BENCH_RUN("stable_sort random (100k)", 3, bench_stable_sort_random);

    printf("\nBenchmark suite complete.\n");
    free(g_bench_random_ints);
    free(g_bench_sorted_ints);
    return 0;
}
