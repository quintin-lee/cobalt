/**
 * @file bench_runner.c
 * @brief Unified benchmark runner — executes all benchmarks with statistics
 */

#include "cobalt/algorithm/regex.h"
#include "cobalt/algorithm/sort.h"
#include "cobalt/container/buffer.h"
#include "cobalt/container/deque.h"
#include "cobalt/container/hashmap.h"
#include "cobalt/container/lfqueue.h"
#include "cobalt/container/lfskiplist.h"
#include "cobalt/container/list.h"
#include "cobalt/container/queue.h"
#include "cobalt/container/set.h"
#include "cobalt/container/stack.h"
#include "cobalt/container/treemap.h"
#include "cobalt/container/vector.h"
#include "cobalt/module/eventloop.h"
#include "cobalt/module/json.h"
#include "cobalt/module/json_sax.h"
#include "cobalt/module/parallel_sort.h"
#include "cobalt/module/shm_channel.h"
#include "cobalt/module/taskgraph.h"
#include "cobalt/module/threadpool.h"
#include "cobalt/platform/shm.h"
#include "cobalt/utils/benchmark.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

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
            uint32_t seed = 42U;
            for (int i = 0; i < BENCH_ITERS; i++) {
                seed                   = seed * 1664525U + 1013904223U;
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

static void bench_parallel_qsort_random(void)
{
    ensure_sort_bench_data();
    int *arr = (int *)malloc(sizeof(int) * BENCH_ITERS);
    if (!arr || !g_bench_random_ints) {
        free(arr);
        return;
    }
    memcpy(arr, g_bench_random_ints, sizeof(int) * BENCH_ITERS);
    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    if (pool) {
        cobalt_parallel_qsort(pool, arr, BENCH_ITERS, sizeof(int), bench_sort_cmp_int);
        cobalt_threadpool_destroy(pool);
    }
    free(arr);
}

static void bench_parallel_stable_sort_random(void)
{
    ensure_sort_bench_data();
    int *arr = (int *)malloc(sizeof(int) * BENCH_ITERS);
    if (!arr || !g_bench_random_ints) {
        free(arr);
        return;
    }
    memcpy(arr, g_bench_random_ints, sizeof(int) * BENCH_ITERS);
    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    if (pool) {
        cobalt_parallel_stable_sort(pool, arr, BENCH_ITERS, sizeof(int), bench_sort_cmp_int);
        cobalt_threadpool_destroy(pool);
    }
    free(arr);
}

static void bench_tg_increment(void *arg)
{
    atomic_int *counter = (atomic_int *)arg;
    atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
}

static void bench_taskgraph_dag(void)
{
    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    if (!pool) {
        return;
    }
    cobalt_taskgraph_t *g = cobalt_taskgraph_create();
    if (!g) {
        cobalt_threadpool_destroy(pool);
        return;
    }

    atomic_int counter;
    atomic_init(&counter, 0);

    const int        tasks = 10000;
    cobalt_task_id_t root  = cobalt_taskgraph_add_node(g, bench_tg_increment, &counter);
    cobalt_task_id_t sink  = cobalt_taskgraph_add_node(g, bench_tg_increment, &counter);

    for (int i = 0; i < tasks; i++) {
        cobalt_task_id_t w = cobalt_taskgraph_add_node(g, bench_tg_increment, &counter);
        cobalt_taskgraph_add_edge(g, root, w);
        cobalt_taskgraph_add_edge(g, w, sink);
    }

    if (cobalt_taskgraph_compile(g) == 0) {
        cobalt_taskgraph_run_and_wait(g, pool);
    }

    cobalt_taskgraph_destroy(g);
    cobalt_threadpool_destroy(pool);
}

static void bench_tp_noop_task(void *arg)
{
    atomic_int *counter = (atomic_int *)arg;
    atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
}

static void bench_threadpool_throughput(void)
{
    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    if (!pool) {
        return;
    }
    atomic_int counter;
    atomic_init(&counter, 0);
    const int tasks = 50000;
    for (int i = 0; i < tasks; i++) {
        cobalt_threadpool_submit(pool, bench_tp_noop_task, &counter);
    }
    cobalt_threadpool_wait_idle(pool);
    cobalt_threadpool_destroy(pool);
}

static void bench_tp_pfor_kernel(size_t index, void *arg)
{
    int *arr   = (int *)arg;
    arr[index] = arr[index] * 1664525U + 1013904223U;
}

static void bench_threadpool_parallel_for(void)
{
    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    if (!pool) {
        return;
    }
    const size_t n   = 100000;
    int         *arr = (int *)malloc(sizeof(int) * n);
    if (arr) {
        for (size_t i = 0; i < n; i++) {
            arr[i] = (int)i;
        }
        cobalt_threadpool_parallel_for(pool, 0, n, 0, bench_tp_pfor_kernel, arr);
        free(arr);
    }
    cobalt_threadpool_destroy(pool);
}

/* ===== EventLoop & Buffer ===== */
static void dummy_runner_timer_cb(uint64_t timer_id, void *user_data)
{
    (void)timer_id;
    (void)user_data;
}

static void bench_eventloop_tw(void)
{
    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    if (!loop) {
        return;
    }
    const int count = 20000;
    uint64_t *ids   = (uint64_t *)malloc(sizeof(uint64_t) * count);
    if (ids) {
        for (int i = 0; i < count; i++) {
            ids[i] = cobalt_eventloop_add_timer(
                loop, (uint64_t)((i % 5000) + 1), 0, dummy_runner_timer_cb, NULL);
        }
        for (int i = 0; i < count / 2; i++) {
            cobalt_eventloop_del_timer(loop, ids[i * 2]);
        }
        free(ids);
    }
    cobalt_eventloop_destroy(loop);
}

static void runner_post_cb(void *arg)
{
    atomic_int *counter = (atomic_int *)arg;
    atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);
}

static void bench_eventloop_post(void)
{
    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    if (!loop) {
        return;
    }
    atomic_int counter;
    atomic_init(&counter, 0);
    const int n = 20000;
    for (int i = 0; i < n; i++) {
        cobalt_eventloop_post(loop, runner_post_cb, &counter);
    }
    while (atomic_load_explicit(&counter, memory_order_relaxed) < n) {
        cobalt_eventloop_iteration(loop);
    }
    cobalt_eventloop_destroy(loop);
}

static void bench_buffer_throughput(void)
{
    cobalt_buffer_t *buf = cobalt_buffer_create(65536);
    if (!buf) {
        return;
    }
    char chunk[1024];
    memset(chunk, 'X', sizeof(chunk));
    for (int i = 0; i < 50000; i++) {
        cobalt_buffer_append(buf, chunk, sizeof(chunk));
        cobalt_buffer_retrieve(buf, sizeof(chunk));
    }
    cobalt_buffer_destroy(buf);
}

/* ===== Regex ===== */
static void bench_regex_redos(void)
{
    cobalt_regex_t *re = cobalt_regex_compile("^(a+)+$", COBALT_REGEX_DEFAULT);
    if (!re) {
        return;
    }
    const char *pathological = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaX";
    for (int i = 0; i < 1000; i++) {
        cobalt_regex_search(re, pathological, NULL, 0);
    }
    cobalt_regex_destroy(re);
}

static void bench_regex_email(void)
{
    cobalt_regex_t *re = cobalt_regex_compile("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,6}$",
                                              COBALT_REGEX_DEFAULT);
    if (!re) {
        return;
    }
    const char *email = "user.name+tag@example-domain.co.uk";
    for (int i = 0; i < 10000; i++) {
        cobalt_regex_match_exact(re, email, NULL, 0);
    }
    cobalt_regex_destroy(re);
}

static void bench_regex_ipv4(void)
{
    cobalt_regex_t *re =
        cobalt_regex_compile("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}", COBALT_REGEX_DEFAULT);
    if (!re) {
        return;
    }
    const char *line = "2026-10-17 12:00:00 IP 192.168.1.100 GET /api/v1/resource HTTP/1.1 200 OK";
    cobalt_regex_capture_t cap;
    for (int i = 0; i < 10000; i++) {
        cobalt_regex_search(re, line, &cap, 1);
    }
    cobalt_regex_destroy(re);
}

static int bench_lfskiplist_cmp(const void *a, const void *b)
{
    intptr_t ia = (intptr_t)a;
    intptr_t ib = (intptr_t)b;
    return (ia < ib) ? -1 : (ia > ib) ? 1 : 0;
}

static void bench_lfskiplist_ops(void)
{
    cobalt_lfskiplist_t *sl = cobalt_lfskiplist_create(bench_lfskiplist_cmp);
    for (int i = 0; i < 50000; i++) {
        cobalt_lfskiplist_insert(sl, (void *)(intptr_t)i, (void *)(intptr_t)i);
    }
    for (int i = 0; i < 50000; i++) {
        void *val = NULL;
        cobalt_lfskiplist_get(sl, (void *)(intptr_t)i, &val);
    }
    cobalt_lfskiplist_destroy(sl);
}

static void cleanup_runner_shm(const char *name)
{
    if (!name) {
        return;
    }
    cobalt_shm_unlink(name);
#ifndef _WIN32
    char        path[256];
    const char *p = name;
    while (*p == '/') {
        p++;
    }
    snprintf(path, sizeof(path), "/dev/shm/%s_notif", p);
    unlink(path);
    snprintf(path, sizeof(path), "/tmp/%s_notif", p);
    unlink(path);
#endif
}

static void bench_shm_channel_ops(void)
{
    const char *name = "/cobalt_bench_runner_shm";
    cleanup_runner_shm(name);

    cobalt_shm_config_t cfg = {
        .name              = name,
        .mode              = COBALT_SHM_MODE_SPSC,
        .wait_policy       = COBALT_SHM_WAIT_SPIN,
        .capacity          = 65536,
        .slot_size         = 0,
        .create_if_missing = true,
        .spin_threshold    = 1000,
    };

    cobalt_shm_channel_t *chan = cobalt_shm_channel_create(&cfg);
    if (!chan) {
        return;
    }

    uint64_t val = 42;
    for (int i = 0; i < 50000; i++) {
        void *wptr = NULL;
        int   rc   = cobalt_shm_channel_reserve(chan, sizeof(val), &wptr);
        (void)rc;
        assert(rc == COBALT_SUCCESS && wptr != NULL);
        *(uint64_t *)wptr = val;
        cobalt_shm_channel_commit(chan, 1, sizeof(val));

        uint16_t    type = 0;
        const void *rptr = NULL;
        size_t      len  = 0;
        rc               = cobalt_shm_channel_peek(chan, &type, &rptr, &len);
        (void)rc;
        assert(rc == COBALT_SUCCESS && rptr != NULL);
        rc = cobalt_shm_channel_consume(chan);
        (void)rc;
        assert(rc == COBALT_SUCCESS);
    }

    cobalt_shm_channel_destroy(chan);
    cleanup_runner_shm(name);
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
        cobalt_bench_result_t results[32];
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
        results[17].name = "threadpool_throughput";
        cobalt_bench_run("threadpool_throughput", 3, bench_threadpool_throughput, &results[17]);
        results[18].name = "threadpool_parallel_for";
        cobalt_bench_run("threadpool_parallel_for", 3, bench_threadpool_parallel_for, &results[18]);
        results[19].name = "eventloop_tw";
        cobalt_bench_run("eventloop_tw", 3, bench_eventloop_tw, &results[19]);
        results[20].name = "eventloop_post";
        cobalt_bench_run("eventloop_post", 3, bench_eventloop_post, &results[20]);
        results[21].name = "buffer_throughput";
        cobalt_bench_run("buffer_throughput", 3, bench_buffer_throughput, &results[21]);
        results[22].name = "parallel_qsort_random";
        cobalt_bench_run("parallel_qsort_random", 3, bench_parallel_qsort_random, &results[22]);
        results[23].name = "parallel_stable_sort_random";
        cobalt_bench_run(
            "parallel_stable_sort_random", 3, bench_parallel_stable_sort_random, &results[23]);
        results[24].name = "taskgraph_dag";
        cobalt_bench_run("taskgraph_dag", 3, bench_taskgraph_dag, &results[24]);
        results[25].name = "regex_redos";
        cobalt_bench_run("regex_redos", 3, bench_regex_redos, &results[25]);
        results[26].name = "regex_email";
        cobalt_bench_run("regex_email", 3, bench_regex_email, &results[26]);
        results[27].name = "regex_ipv4";
        cobalt_bench_run("regex_ipv4", 3, bench_regex_ipv4, &results[27]);
        results[28].name = "lfskiplist_ops";
        cobalt_bench_run("lfskiplist_ops", 3, bench_lfskiplist_ops, &results[28]);
        results[29].name = "shm_channel_ops";
        cobalt_bench_run("shm_channel_ops", 3, bench_shm_channel_ops, &results[29]);
        results[30].name = NULL;

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
    BENCH_RUN("parallel_qsort random (100k)", 3, bench_parallel_qsort_random);
    BENCH_RUN("parallel_stable_sort random (100k)", 3, bench_parallel_stable_sort_random);

    printf("--- ThreadPool & TaskGraph ---\n");
    BENCH_RUN("threadpool throughput (50k)", 3, bench_threadpool_throughput);
    BENCH_RUN("threadpool parallel_for (100k)", 3, bench_threadpool_parallel_for);
    BENCH_RUN("taskgraph DAG (10k tasks)", 3, bench_taskgraph_dag);

    printf("--- EventLoop & Buffer ---\n");
    BENCH_RUN("eventloop timing wheel (20k)", 3, bench_eventloop_tw);
    BENCH_RUN("eventloop post tasks (20k)", 3, bench_eventloop_post);
    BENCH_RUN("buffer append/retrieve (50MB)", 3, bench_buffer_throughput);

    printf("--- Regex Engine ---\n");
    BENCH_RUN("regex redos immunity (1k)", 3, bench_regex_redos);
    BENCH_RUN("regex email validation (10k)", 3, bench_regex_email);
    BENCH_RUN("regex ipv4 extraction (10k)", 3, bench_regex_ipv4);

    printf("--- Lock-Free SkipList ---\n");
    BENCH_RUN("lfskiplist put/get (50k)", 3, bench_lfskiplist_ops);

    printf("--- Shared Memory IPC ---\n");
    BENCH_RUN("shm spsc channel (50k)", 3, bench_shm_channel_ops);

    printf("\nBenchmark suite complete.\n");
    free(g_bench_random_ints);
    free(g_bench_sorted_ints);
    return 0;
}
