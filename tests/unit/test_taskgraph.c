/**
 * @file test_taskgraph.c
 * @brief Unit tests for Directed Acyclic Graph (DAG) task scheduler
 */

#include "cobalt/module/taskgraph.h"
#include "cobalt/runtime/error.h"
#include "test_framework.h"
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

void test_taskgraph(void)
{
    printf("=== Starting TaskGraph Tests ===\n");
    test_taskgraph_construction();
    test_taskgraph_compile_valid();
    test_taskgraph_cycle_detection();
    printf("=== TaskGraph Tests Completed Successfully ===\n");
}
