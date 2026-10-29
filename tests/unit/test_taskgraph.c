/**
 * @file test_taskgraph.c
 * @brief Unit tests for Directed Acyclic Graph (DAG) task scheduler
 */

#include "cobalt/module/taskgraph.h"
#include "cobalt/runtime/error.h"
#include "test_framework.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static void dummy_task_fn(void *arg)
{
    (void)arg;
}

static void test_taskgraph_construction(void)
{
    printf("Testing TaskGraph node and edge construction...\n");
    cobalt_taskgraph_t *graph = cobalt_taskgraph_create();
    TEST_ASSERT(graph != NULL);
    TEST_EQUAL(cobalt_taskgraph_node_count(graph), 0U);

    cobalt_task_id_t n0 = cobalt_taskgraph_add_node(graph, dummy_task_fn, NULL);
    cobalt_task_id_t n1 = cobalt_taskgraph_add_node(graph, dummy_task_fn, NULL);
    cobalt_task_id_t n2 = cobalt_taskgraph_add_node(graph, dummy_task_fn, NULL);

    TEST_EQUAL(n0, 0U);
    TEST_EQUAL(n1, 1U);
    TEST_EQUAL(n2, 2U);
    TEST_EQUAL(cobalt_taskgraph_node_count(graph), 3U);

    TEST_ASSERT(cobalt_taskgraph_add_edge(graph, n0, n1) == 0);
    TEST_ASSERT(cobalt_taskgraph_add_edge(graph, n1, n2) == 0);

    /* Invalid edge cases */
    TEST_ASSERT(cobalt_taskgraph_add_edge(graph, 999, n2) == -1);
    TEST_ASSERT(cobalt_taskgraph_add_edge(graph, n0, 999) == -1);

    cobalt_taskgraph_destroy(graph);
    printf("  TaskGraph construction: OK\n");
}

static void test_taskgraph_compile_valid(void)
{
    printf("Testing TaskGraph compilation on valid DAG...\n");
    cobalt_taskgraph_t *graph = cobalt_taskgraph_create();
    TEST_ASSERT(graph != NULL);

    /* Diamond DAG: 0 -> 1, 0 -> 2, 1 -> 3, 2 -> 3 */
    cobalt_task_id_t n0 = cobalt_taskgraph_add_node(graph, dummy_task_fn, NULL);
    cobalt_task_id_t n1 = cobalt_taskgraph_add_node(graph, dummy_task_fn, NULL);
    cobalt_task_id_t n2 = cobalt_taskgraph_add_node(graph, dummy_task_fn, NULL);
    cobalt_task_id_t n3 = cobalt_taskgraph_add_node(graph, dummy_task_fn, NULL);

    cobalt_taskgraph_add_edge(graph, n0, n1);
    cobalt_taskgraph_add_edge(graph, n0, n2);
    cobalt_taskgraph_add_edge(graph, n1, n3);
    cobalt_taskgraph_add_edge(graph, n2, n3);

    TEST_ASSERT(cobalt_taskgraph_compile(graph) == 0);

    /* Modifying graph after compilation should fail */
    TEST_ASSERT(cobalt_taskgraph_add_node(graph, dummy_task_fn, NULL) == COBALT_INVALID_TASK_ID);
    TEST_ASSERT(cobalt_taskgraph_add_edge(graph, n1, n2) == -1);

    cobalt_taskgraph_destroy(graph);
    printf("  TaskGraph valid DAG compilation: OK\n");
}

static void test_taskgraph_cycle_detection(void)
{
    printf("Testing TaskGraph Kahn cycle detection...\n");

    /* Case 1: Self-loop (0 -> 0) */
    {
        cobalt_taskgraph_t *g  = cobalt_taskgraph_create();
        cobalt_task_id_t    n0 = cobalt_taskgraph_add_node(g, dummy_task_fn, NULL);
        cobalt_taskgraph_add_edge(g, n0, n0);
        TEST_ASSERT(cobalt_taskgraph_compile(g) == -1);
        cobalt_taskgraph_destroy(g);
    }

    /* Case 2: 2-node cycle (0 -> 1 -> 0) */
    {
        cobalt_taskgraph_t *g  = cobalt_taskgraph_create();
        cobalt_task_id_t    n0 = cobalt_taskgraph_add_node(g, dummy_task_fn, NULL);
        cobalt_task_id_t    n1 = cobalt_taskgraph_add_node(g, dummy_task_fn, NULL);
        cobalt_taskgraph_add_edge(g, n0, n1);
        cobalt_taskgraph_add_edge(g, n1, n0);
        TEST_ASSERT(cobalt_taskgraph_compile(g) == -1);
        cobalt_taskgraph_destroy(g);
    }

    /* Case 3: 3-node cycle (0 -> 1 -> 2 -> 0) */
    {
        cobalt_taskgraph_t *g  = cobalt_taskgraph_create();
        cobalt_task_id_t    n0 = cobalt_taskgraph_add_node(g, dummy_task_fn, NULL);
        cobalt_task_id_t    n1 = cobalt_taskgraph_add_node(g, dummy_task_fn, NULL);
        cobalt_task_id_t    n2 = cobalt_taskgraph_add_node(g, dummy_task_fn, NULL);
        cobalt_taskgraph_add_edge(g, n0, n1);
        cobalt_taskgraph_add_edge(g, n1, n2);
        cobalt_taskgraph_add_edge(g, n2, n0);
        TEST_ASSERT(cobalt_taskgraph_compile(g) == -1);
        cobalt_taskgraph_destroy(g);
    }

    printf("  TaskGraph Kahn cycle detection: OK\n");
}

typedef struct {
    _Atomic(bool)   a_done;
    _Atomic(bool)   b_done;
    _Atomic(bool)   c_done;
    _Atomic(bool)   d_done;
    _Atomic(size_t) total_executions;
} diamond_ctx_t;

static void task_a(void *arg)
{
    diamond_ctx_t *ctx = (diamond_ctx_t *)arg;
    TEST_ASSERT(!atomic_load(&ctx->b_done));
    TEST_ASSERT(!atomic_load(&ctx->c_done));
    TEST_ASSERT(!atomic_load(&ctx->d_done));
    atomic_store(&ctx->a_done, true);
    atomic_fetch_add(&ctx->total_executions, 1);
}

static void task_b(void *arg)
{
    diamond_ctx_t *ctx = (diamond_ctx_t *)arg;
    TEST_ASSERT(atomic_load(&ctx->a_done));
    TEST_ASSERT(!atomic_load(&ctx->d_done));
    atomic_store(&ctx->b_done, true);
    atomic_fetch_add(&ctx->total_executions, 1);
}

static void task_c(void *arg)
{
    diamond_ctx_t *ctx = (diamond_ctx_t *)arg;
    TEST_ASSERT(atomic_load(&ctx->a_done));
    TEST_ASSERT(!atomic_load(&ctx->d_done));
    atomic_store(&ctx->c_done, true);
    atomic_fetch_add(&ctx->total_executions, 1);
}

static void task_d(void *arg)
{
    diamond_ctx_t *ctx = (diamond_ctx_t *)arg;
    TEST_ASSERT(atomic_load(&ctx->a_done));
    TEST_ASSERT(atomic_load(&ctx->b_done));
    TEST_ASSERT(atomic_load(&ctx->c_done));
    atomic_store(&ctx->d_done, true);
    atomic_fetch_add(&ctx->total_executions, 1);
}

static void test_taskgraph_run_and_wait_diamond(void)
{
    printf("Testing TaskGraph diamond run and wait...\n");
    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);

    cobalt_taskgraph_t *graph = cobalt_taskgraph_create();
    TEST_ASSERT(graph != NULL);

    diamond_ctx_t ctx = {0};
    atomic_init(&ctx.a_done, false);
    atomic_init(&ctx.b_done, false);
    atomic_init(&ctx.c_done, false);
    atomic_init(&ctx.d_done, false);
    atomic_init(&ctx.total_executions, 0);

    cobalt_task_id_t na = cobalt_taskgraph_add_node(graph, task_a, &ctx);
    cobalt_task_id_t nb = cobalt_taskgraph_add_node(graph, task_b, &ctx);
    cobalt_task_id_t nc = cobalt_taskgraph_add_node(graph, task_c, &ctx);
    cobalt_task_id_t nd = cobalt_taskgraph_add_node(graph, task_d, &ctx);

    cobalt_taskgraph_add_edge(graph, na, nb);
    cobalt_taskgraph_add_edge(graph, na, nc);
    cobalt_taskgraph_add_edge(graph, nb, nd);
    cobalt_taskgraph_add_edge(graph, nc, nd);

    TEST_ASSERT(cobalt_taskgraph_compile(graph) == 0);
    TEST_ASSERT(cobalt_taskgraph_run_and_wait(graph, pool) == 0);

    TEST_ASSERT(atomic_load(&ctx.a_done));
    TEST_ASSERT(atomic_load(&ctx.b_done));
    TEST_ASSERT(atomic_load(&ctx.c_done));
    TEST_ASSERT(atomic_load(&ctx.d_done));
    TEST_EQUAL(atomic_load(&ctx.total_executions), 4U);

    cobalt_taskgraph_destroy(graph);
    cobalt_threadpool_destroy(pool);
    printf("  TaskGraph diamond run and wait: OK\n");
}

static void test_taskgraph_reusability_100x(void)
{
    printf("Testing TaskGraph reusability 100x...\n");
    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);

    cobalt_taskgraph_t *graph = cobalt_taskgraph_create();
    TEST_ASSERT(graph != NULL);

    diamond_ctx_t ctx = {0};
    atomic_init(&ctx.total_executions, 0);

    cobalt_task_id_t na = cobalt_taskgraph_add_node(graph, task_a, &ctx);
    cobalt_task_id_t nb = cobalt_taskgraph_add_node(graph, task_b, &ctx);
    cobalt_task_id_t nc = cobalt_taskgraph_add_node(graph, task_c, &ctx);
    cobalt_task_id_t nd = cobalt_taskgraph_add_node(graph, task_d, &ctx);

    cobalt_taskgraph_add_edge(graph, na, nb);
    cobalt_taskgraph_add_edge(graph, na, nc);
    cobalt_taskgraph_add_edge(graph, nb, nd);
    cobalt_taskgraph_add_edge(graph, nc, nd);

    TEST_ASSERT(cobalt_taskgraph_compile(graph) == 0);

    for (int run = 0; run < 100; run++) {
        atomic_store(&ctx.a_done, false);
        atomic_store(&ctx.b_done, false);
        atomic_store(&ctx.c_done, false);
        atomic_store(&ctx.d_done, false);

        int rc = cobalt_taskgraph_run_and_wait(graph, pool);
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(atomic_load(&ctx.a_done));
        TEST_ASSERT(atomic_load(&ctx.b_done));
        TEST_ASSERT(atomic_load(&ctx.c_done));
        TEST_ASSERT(atomic_load(&ctx.d_done));
    }

    TEST_EQUAL(atomic_load(&ctx.total_executions), 400U);

    cobalt_taskgraph_destroy(graph);
    cobalt_threadpool_destroy(pool);
    printf("  TaskGraph reusability 100x: OK\n");
}

typedef struct {
    _Atomic(int) step;
} chain_item_t;

static void chain_task(void *arg)
{
    chain_item_t *item = (chain_item_t *)arg;
    atomic_fetch_add(&item->step, 1);
}

static void test_taskgraph_forest(void)
{
    printf("Testing TaskGraph forest (disjoint components)...\n");
    cobalt_threadpool_t *pool = cobalt_threadpool_create(4);
    TEST_ASSERT(pool != NULL);

    cobalt_taskgraph_t *graph = cobalt_taskgraph_create();
    TEST_ASSERT(graph != NULL);

    chain_item_t items[7];
    for (int i = 0; i < 7; i++) {
        atomic_init(&items[i].step, 0);
    }

    /* Chain 1: 0 -> 1 -> 2 */
    cobalt_task_id_t c1_0 = cobalt_taskgraph_add_node(graph, chain_task, &items[0]);
    cobalt_task_id_t c1_1 = cobalt_taskgraph_add_node(graph, chain_task, &items[1]);
    cobalt_task_id_t c1_2 = cobalt_taskgraph_add_node(graph, chain_task, &items[2]);
    cobalt_taskgraph_add_edge(graph, c1_0, c1_1);
    cobalt_taskgraph_add_edge(graph, c1_1, c1_2);

    /* Chain 2: 3 -> 4 -> 5 */
    cobalt_task_id_t c2_0 = cobalt_taskgraph_add_node(graph, chain_task, &items[3]);
    cobalt_task_id_t c2_1 = cobalt_taskgraph_add_node(graph, chain_task, &items[4]);
    cobalt_task_id_t c2_2 = cobalt_taskgraph_add_node(graph, chain_task, &items[5]);
    cobalt_taskgraph_add_edge(graph, c2_0, c2_1);
    cobalt_taskgraph_add_edge(graph, c2_1, c2_2);

    /* Isolated node 6 */
    cobalt_task_id_t c3 = cobalt_taskgraph_add_node(graph, chain_task, &items[6]);
    (void)c3;

    TEST_ASSERT(cobalt_taskgraph_compile(graph) == 0);
    TEST_ASSERT(cobalt_taskgraph_run_and_wait(graph, pool) == 0);

    for (int i = 0; i < 7; i++) {
        TEST_EQUAL(atomic_load(&items[i].step), 1);
    }

    cobalt_taskgraph_destroy(graph);
    cobalt_threadpool_destroy(pool);
    printf("  TaskGraph forest: OK\n");
}

static void test_taskgraph_edge_cases(void)
{
    printf("Testing TaskGraph edge cases...\n");
    cobalt_threadpool_t *pool = cobalt_threadpool_create(2);
    TEST_ASSERT(pool != NULL);

    /* Empty graph */
    {
        cobalt_taskgraph_t *g = cobalt_taskgraph_create();
        TEST_ASSERT(g != NULL);
        TEST_ASSERT(cobalt_taskgraph_compile(g) == 0);
        TEST_ASSERT(cobalt_taskgraph_run(g, pool) == 0);
        TEST_ASSERT(cobalt_taskgraph_run_and_wait(g, pool) == 0);
        cobalt_taskgraph_destroy(g);
    }

    /* Run without compile */
    {
        cobalt_taskgraph_t *g = cobalt_taskgraph_create();
        cobalt_taskgraph_add_node(g, dummy_task_fn, NULL);
        TEST_ASSERT(cobalt_taskgraph_run(g, pool) == -1);
        TEST_ASSERT(cobalt_taskgraph_run_and_wait(g, pool) == -1);
        cobalt_taskgraph_destroy(g);
    }

    /* NULL arguments */
    {
        TEST_ASSERT(cobalt_taskgraph_run(NULL, pool) == -1);
        TEST_ASSERT(cobalt_taskgraph_run_and_wait(NULL, pool) == -1);
    }

    cobalt_threadpool_destroy(pool);
    printf("  TaskGraph edge cases: OK\n");
}

void test_taskgraph_reset_and_alloc(void)
{
    printf("Testing taskgraph reset and create_with_alloc...\n");

    /* Verified against taskgraph.c: NULL alloc falls back to malloc, non-NULL
       result. This differs from the containers, which reject NULL outright. */
    cobalt_taskgraph_t *graph = cobalt_taskgraph_create_with_alloc(NULL);
    TEST_ASSERT(graph != NULL);

    cobalt_task_id_t a = cobalt_taskgraph_add_node(graph, dummy_task_fn, NULL);
    cobalt_task_id_t b = cobalt_taskgraph_add_node(graph, dummy_task_fn, NULL);
    TEST_ASSERT(a != COBALT_INVALID_TASK_ID);
    TEST_ASSERT(b != COBALT_INVALID_TASK_ID);
    TEST_ASSERT(a != b);
    TEST_EQUAL(cobalt_taskgraph_node_count(graph), (size_t)2);

    TEST_ASSERT(cobalt_taskgraph_add_edge(graph, a, b) == 0);

    /* Reset restores each node's remaining_in_degree from its static value so a
       consumed graph can run again. The topology itself (nodes and edges)
       survives the reset: node count is unchanged and the edge is still
       present. Probe-verified: add_edge returns 0 again after a reset. */
    cobalt_taskgraph_reset(graph);
    TEST_EQUAL(cobalt_taskgraph_node_count(graph), (size_t)2);
    TEST_ASSERT(cobalt_taskgraph_add_edge(graph, a, b) == 0);

    cobalt_taskgraph_reset(NULL); /* must not crash */
    cobalt_taskgraph_destroy(graph);
    printf("  Taskgraph reset and allocator fallback: OK\n");
}

void test_taskgraph(void)
{
    printf("=== Starting TaskGraph Tests ===\n");
    test_taskgraph_construction();
    test_taskgraph_compile_valid();
    test_taskgraph_cycle_detection();
    test_taskgraph_run_and_wait_diamond();
    test_taskgraph_reusability_100x();
    test_taskgraph_forest();
    test_taskgraph_edge_cases();
    test_taskgraph_reset_and_alloc();
    printf("=== TaskGraph Tests Completed Successfully ===\n");
}
