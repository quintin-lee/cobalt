/**
 * @file benchmark_threadsafewrapper.c
 * @brief Benchmarks for thread-safe container wrappers
 * @details Compares core containers vs thread-safe wrappers in single-threaded
 *          mode (overhead measurement) and under multi-threaded contention.
 */

#include "cobalt/container/threadsafewrapper.h"
#include "cobalt/utils/benchmark.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCH_SINGLE_ITERS 100000
#define BENCH_THREAD_ITERS 50000
#define BENCH_NTHREADS 8

typedef struct {
    cobalt_tshashmap_t *map;
    int                 tid;
} hashmap_thread_arg_t;

/* ===== Single-threaded: core vs wrapper overhead ===== */

static void bench_core_vector_push(void)
{
    cobalt_vector_t *v   = cobalt_vector_create(64);
    int              val = 42;
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        cobalt_vector_push(v, &val);
    }
    cobalt_vector_destroy(v);
}

static void bench_ts_vector_push(void)
{
    cobalt_tsvector_t *v   = cobalt_tsvector_create(64);
    int                val = 42;
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        cobalt_tsvector_push(v, &val);
    }
    cobalt_tsvector_destroy(v);
}

static void bench_core_vector_get(void)
{
    cobalt_vector_t *v   = cobalt_vector_create(BENCH_SINGLE_ITERS);
    int              val = 42;
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        cobalt_vector_push(v, &val);
    }
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        cobalt_vector_get(v, i);
    }
    cobalt_vector_destroy(v);
}

static void bench_ts_vector_get(void)
{
    cobalt_tsvector_t *v   = cobalt_tsvector_create(BENCH_SINGLE_ITERS);
    int                val = 42;
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        cobalt_tsvector_push(v, &val);
    }
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        cobalt_tsvector_get(v, i);
    }
    cobalt_tsvector_destroy(v);
}

static void bench_core_hashmap_put(void)
{
    cobalt_hashmap_t *map = cobalt_hashmap_create(256);
    char              key[32];
    int               val = 42;
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_hashmap_put(map, key, &val);
    }
    cobalt_hashmap_destroy(map);
}

static void bench_ts_hashmap_put(void)
{
    cobalt_tshashmap_t *map = cobalt_tshashmap_create(256);
    char                key[32];
    int                 val = 42;
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_tshashmap_put(map, key, &val);
    }
    cobalt_tshashmap_destroy(map);
}

static void bench_core_hashmap_get(void)
{
    cobalt_hashmap_t *map = cobalt_hashmap_create(256);
    char              key[32];
    int               val = 42;
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_hashmap_put(map, key, &val);
    }
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_hashmap_get(map, key);
    }
    cobalt_hashmap_destroy(map);
}

static void bench_ts_hashmap_get(void)
{
    cobalt_tshashmap_t *map = cobalt_tshashmap_create(256);
    char                key[32];
    int                 val = 42;
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_tshashmap_put(map, key, &val);
    }
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        snprintf(key, sizeof(key), "key_%05d", i);
        cobalt_tshashmap_get(map, key);
    }
    cobalt_tshashmap_destroy(map);
}

static void bench_core_list_push(void)
{
    cobalt_list_t *list = cobalt_list_create();
    int            val  = 42;
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        cobalt_list_push_back(list, &val);
    }
    cobalt_list_destroy(list);
}

static void bench_ts_list_push(void)
{
    cobalt_tslist_t *list = cobalt_tslist_create();
    int              val  = 42;
    for (int i = 0; i < BENCH_SINGLE_ITERS; i++) {
        cobalt_tslist_push_back(list, &val);
    }
    cobalt_tslist_destroy(list);
}

/* ===== Multi-threaded: thread-safe container under contention ===== */

static void *mt_vector_push_thread(void *arg)
{
    cobalt_tsvector_t *v   = (cobalt_tsvector_t *)arg;
    int                val = 42;
    for (int i = 0; i < BENCH_THREAD_ITERS; i++) {
        cobalt_tsvector_push(v, &val);
    }
    return NULL;
}

static void *mt_vector_get_thread(void *arg)
{
    cobalt_tsvector_t *v = (cobalt_tsvector_t *)arg;
    for (int i = 0; i < BENCH_THREAD_ITERS; i++) {
        cobalt_tsvector_get(v, i % BENCH_THREAD_ITERS);
    }
    return NULL;
}

static void bench_mt_vector_push(void)
{
    cobalt_tsvector_t *v = cobalt_tsvector_create(1024);
    pthread_t          threads[BENCH_NTHREADS];
    for (int i = 0; i < BENCH_NTHREADS; i++) {
        pthread_create(&threads[i], NULL, mt_vector_push_thread, v);
    }
    for (int i = 0; i < BENCH_NTHREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    cobalt_tsvector_destroy(v);
}

static void bench_mt_vector_get(void)
{
    cobalt_tsvector_t *v   = cobalt_tsvector_create(1024);
    int                val = 42;
    for (int i = 0; i < BENCH_THREAD_ITERS; i++) {
        cobalt_tsvector_push(v, &val);
    }
    pthread_t threads[BENCH_NTHREADS];
    for (int i = 0; i < BENCH_NTHREADS; i++) {
        pthread_create(&threads[i], NULL, mt_vector_get_thread, v);
    }
    for (int i = 0; i < BENCH_NTHREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    cobalt_tsvector_destroy(v);
}

static void *mt_hashmap_put_thread(void *arg)
{
    hashmap_thread_arg_t *a   = (hashmap_thread_arg_t *)arg;
    cobalt_tshashmap_t   *map = a->map;
    int                   tid = a->tid;
    for (int i = 0; i < BENCH_THREAD_ITERS; i++) {
        char key[32];
        int *val = (int *)malloc(sizeof(int));
        if (!val) {
            return NULL;
        }
        *val = i;
        snprintf(key, sizeof(key), "key_%d_%d", tid, i);
        cobalt_tshashmap_put(map, key, val);
    }
    return NULL;
}

static void *mt_hashmap_get_thread(void *arg)
{
    hashmap_thread_arg_t *a   = (hashmap_thread_arg_t *)arg;
    cobalt_tshashmap_t   *map = a->map;
    int                   tid = a->tid;
    for (int i = 0; i < BENCH_THREAD_ITERS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d_%d", tid, i);
        cobalt_tshashmap_get(map, key);
    }
    return NULL;
}

static void bench_mt_hashmap_put(void)
{
    cobalt_tshashmap_t *map = cobalt_tshashmap_create(256);
    pthread_t           threads[BENCH_NTHREADS];
    for (int i = 0; i < BENCH_NTHREADS; i++) {
        hashmap_thread_arg_t arg = {.map = map, .tid = i};
        pthread_create(&threads[i], NULL, mt_hashmap_put_thread, &arg);
    }
    for (int i = 0; i < BENCH_NTHREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    /* Cleanup */
    for (int t = 0; t < BENCH_NTHREADS; t++) {
        for (int i = 0; i < BENCH_THREAD_ITERS; i++) {
            char key[32];
            snprintf(key, sizeof(key), "key_%d_%d", t + 1, i);
            int *val = (int *)cobalt_tshashmap_get(map, key);
            if (val) {
                free(val);
            }
        }
    }
    cobalt_tshashmap_destroy(map);
}

static void bench_mt_hashmap_get(void)
{
    cobalt_tshashmap_t *map = cobalt_tshashmap_create(256);
    for (int t = 0; t < BENCH_NTHREADS; t++) {
        for (int i = 0; i < BENCH_THREAD_ITERS; i++) {
            char key[32];
            int *val = (int *)malloc(sizeof(int));
            if (!val) {
                continue;
            }
            *val = i;
            snprintf(key, sizeof(key), "key_%d_%d", t, i);
            cobalt_tshashmap_put(map, key, val);
        }
    }
    pthread_t threads[BENCH_NTHREADS];
    for (int i = 0; i < BENCH_NTHREADS; i++) {
        hashmap_thread_arg_t arg = {.map = map, .tid = i};
        pthread_create(&threads[i], NULL, mt_hashmap_get_thread, &arg);
    }
    for (int i = 0; i < BENCH_NTHREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    /* Cleanup */
    for (int t = 0; t < BENCH_NTHREADS; t++) {
        for (int i = 0; i < BENCH_THREAD_ITERS; i++) {
            char key[32];
            snprintf(key, sizeof(key), "key_%d_%d", t, i);
            int *val = (int *)cobalt_tshashmap_get(map, key);
            if (val) {
                free(val);
            }
        }
    }
    cobalt_tshashmap_destroy(map);
}

static void *mt_list_push_thread(void *arg)
{
    cobalt_tslist_t *list = (cobalt_tslist_t *)arg;
    for (int i = 0; i < BENCH_THREAD_ITERS; i++) {
        int *val = (int *)malloc(sizeof(int));
        if (!val) {
            return NULL;
        }
        *val = i;
        cobalt_tslist_push_back(list, val);
    }
    return NULL;
}

static void bench_mt_list_push(void)
{
    cobalt_tslist_t *list = cobalt_tslist_create();
    pthread_t        threads[BENCH_NTHREADS];
    for (int i = 0; i < BENCH_NTHREADS; i++) {
        pthread_create(&threads[i], NULL, mt_list_push_thread, list);
    }
    for (int i = 0; i < BENCH_NTHREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    /* Cleanup */
    while (cobalt_tslist_size(list) > 0) {
        int *p = (int *)cobalt_tslist_pop_front(list);
        if (p) {
            free(p);
        }
    }
    cobalt_tslist_destroy(list);
}

int main(int argc, char *argv[])
{
    (void)argc;
    int json_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = 1;
        }
    }

    if (json_mode) {
        cobalt_bench_result_t results[12];
        memset(results, 0, sizeof(results));

        results[0].name = "vector_push_core";
        cobalt_bench_run("vector_push_core", 3, bench_core_vector_push, &results[0]);
        results[1].name = "vector_push_ts";
        cobalt_bench_run("vector_push_ts", 3, bench_ts_vector_push, &results[1]);
        results[2].name = "vector_get_core";
        cobalt_bench_run("vector_get_core", 3, bench_core_vector_get, &results[2]);
        results[3].name = "vector_get_ts";
        cobalt_bench_run("vector_get_ts", 3, bench_ts_vector_get, &results[3]);
        results[4].name = "hashmap_put_core";
        cobalt_bench_run("hashmap_put_core", 3, bench_core_hashmap_put, &results[4]);
        results[5].name = "hashmap_put_ts";
        cobalt_bench_run("hashmap_put_ts", 3, bench_ts_hashmap_put, &results[5]);
        results[6].name = "hashmap_get_core";
        cobalt_bench_run("hashmap_get_core", 3, bench_core_hashmap_get, &results[6]);
        results[7].name = "hashmap_get_ts";
        cobalt_bench_run("hashmap_get_ts", 3, bench_ts_hashmap_get, &results[7]);
        results[8].name = "list_push_core";
        cobalt_bench_run("list_push_core", 3, bench_core_list_push, &results[8]);
        results[9].name = "list_push_ts";
        cobalt_bench_run("list_push_ts", 3, bench_ts_list_push, &results[9]);
        results[10].name = "mt_vector_push";
        cobalt_bench_run("mt_vector_push", 3, bench_mt_vector_push, &results[10]);
        results[11].name = NULL;

        cobalt_bench_output_json(results, stdout);
        return 0;
    }

    printf("Cobalt Thread-Safe Container Benchmarks\n");
    printf("========================================\n\n");

    printf("--- Single-threaded: Core vs Thread-Safe Overhead ---\n\n");
    printf("  Vector push (%d iters):\n", BENCH_SINGLE_ITERS);
    BENCH_RUN("    core", 3, bench_core_vector_push);
    BENCH_RUN("    ts     ", 3, bench_ts_vector_push);
    printf("  Vector get (%d iters):\n", BENCH_SINGLE_ITERS);
    BENCH_RUN("    core", 3, bench_core_vector_get);
    BENCH_RUN("    ts     ", 3, bench_ts_vector_get);
    printf("  HashMap put (%d iters):\n", BENCH_SINGLE_ITERS);
    BENCH_RUN("    core", 3, bench_core_hashmap_put);
    BENCH_RUN("    ts     ", 3, bench_ts_hashmap_put);
    printf("  HashMap get (%d iters):\n", BENCH_SINGLE_ITERS);
    BENCH_RUN("    core", 3, bench_core_hashmap_get);
    BENCH_RUN("    ts     ", 3, bench_ts_hashmap_get);
    printf("  List push (%d iters):\n", BENCH_SINGLE_ITERS);
    BENCH_RUN("    core", 3, bench_core_list_push);
    BENCH_RUN("    ts     ", 3, bench_ts_list_push);

    printf(
        "\n--- Multi-threaded: %d threads x %d iters ---\n\n", BENCH_NTHREADS, BENCH_THREAD_ITERS);
    BENCH_RUN("mt_vector_push", 3, bench_mt_vector_push);
    BENCH_RUN("mt_vector_get", 3, bench_mt_vector_get);
    BENCH_RUN("mt_hashmap_put", 3, bench_mt_hashmap_put);
    BENCH_RUN("mt_hashmap_get", 3, bench_mt_hashmap_get);
    BENCH_RUN("mt_list_push", 3, bench_mt_list_push);

    printf("\nBenchmark suite complete.\n");
    return 0;
}
