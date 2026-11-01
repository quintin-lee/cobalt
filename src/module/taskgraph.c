/**
 * @file taskgraph.c
 * @brief Reusable event-driven Directed Acyclic Graph (DAG) task scheduler
 */

#include "cobalt/module/taskgraph.h"
#include "cobalt/container/vector.h"
#include "cobalt/runtime/error.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct cobalt_taskgraph cobalt_taskgraph_t;

typedef struct {
    cobalt_taskgraph_t *graph;
    cobalt_task_id_t    id;
    cobalt_task_fn_t    fn;
    void               *ctx;
    size_t              static_in_degree;
    _Atomic(size_t)     remaining_in_degree;
    cobalt_vector_t    *successors;
} cobalt_task_node_t;

struct cobalt_taskgraph {
    cobalt_vector_t     *nodes;
    cobalt_vector_t     *root_nodes;
    bool                 compiled;
    cobalt_waitgroup_t  *wg;
    cobalt_threadpool_t *bound_pool;
    cobalt_allocator_t  *alloc;
};

cobalt_taskgraph_t *cobalt_taskgraph_create_with_alloc(cobalt_allocator_t *alloc)
{
    cobalt_taskgraph_t *graph = NULL;
    if (alloc && alloc->alloc) {
        graph = (cobalt_taskgraph_t *)alloc->alloc(alloc, sizeof(cobalt_taskgraph_t));
    } else {
        graph = (cobalt_taskgraph_t *)malloc(sizeof(cobalt_taskgraph_t));
    }
    if (!graph) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    graph->nodes      = cobalt_vector_create(16);
    graph->root_nodes = cobalt_vector_create(8);
    graph->compiled   = false;
    graph->wg         = NULL;
    graph->bound_pool = NULL;
    graph->alloc      = alloc;

    if (!graph->nodes || !graph->root_nodes) {
        cobalt_taskgraph_destroy(graph);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    return graph;
}

cobalt_taskgraph_t *cobalt_taskgraph_create(void)
{
    return cobalt_taskgraph_create_with_alloc(NULL);
}

void cobalt_taskgraph_destroy(cobalt_taskgraph_t *graph)
{
    if (!graph) {
        return;
    }

    if (graph->nodes) {
        size_t n = cobalt_vector_size(graph->nodes);
        for (size_t i = 0; i < n; i++) {
            cobalt_task_node_t *node = (cobalt_task_node_t *)cobalt_vector_get(graph->nodes, i);
            if (node) {
                if (node->successors) {
                    cobalt_vector_destroy(node->successors);
                }
                if (graph->alloc && graph->alloc->free) {
                    graph->alloc->free(graph->alloc, node);
                } else {
                    free(node);
                }
            }
        }
        cobalt_vector_destroy(graph->nodes);
    }

    if (graph->root_nodes) {
        cobalt_vector_destroy(graph->root_nodes);
    }

    if (graph->wg) {
        cobalt_waitgroup_destroy(graph->wg);
    }

    if (graph->alloc && graph->alloc->free) {
        graph->alloc->free(graph->alloc, graph);
    } else {
        free(graph);
    }
}

cobalt_task_id_t
cobalt_taskgraph_add_node(cobalt_taskgraph_t *graph, cobalt_task_fn_t fn, void *ctx)
{
    if (!graph || !fn || graph->compiled) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_INVALID_TASK_ID;
    }

    cobalt_task_node_t *node = NULL;
    if (graph->alloc && graph->alloc->alloc) {
        node = (cobalt_task_node_t *)graph->alloc->alloc(graph->alloc, sizeof(cobalt_task_node_t));
    } else {
        node = (cobalt_task_node_t *)malloc(sizeof(cobalt_task_node_t));
    }
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_INVALID_TASK_ID;
    }

    node->graph            = graph;
    node->id               = cobalt_vector_size(graph->nodes);
    node->fn               = fn;
    node->ctx              = ctx;
    node->static_in_degree = 0;
    atomic_init(&node->remaining_in_degree, 0);
    node->successors = cobalt_vector_create(4);

    if (!node->successors || cobalt_vector_push(graph->nodes, node) != 0) {
        if (node->successors) {
            cobalt_vector_destroy(node->successors);
        }
        if (graph->alloc && graph->alloc->free) {
            graph->alloc->free(graph->alloc, node);
        } else {
            free(node);
        }
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_INVALID_TASK_ID;
    }

    return node->id;
}

int cobalt_taskgraph_add_edge(cobalt_taskgraph_t *graph,
                              cobalt_task_id_t    from_id,
                              cobalt_task_id_t    to_id)
{
    if (!graph || graph->compiled) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }

    size_t count = cobalt_vector_size(graph->nodes);
    if (from_id >= count || to_id >= count) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }

    cobalt_task_node_t *from_node =
        (cobalt_task_node_t *)cobalt_vector_get_unchecked(graph->nodes, from_id);
    cobalt_task_node_t *to_node =
        (cobalt_task_node_t *)cobalt_vector_get_unchecked(graph->nodes, to_id);

    /* Deduplicate existing edge */
    size_t succ_count = cobalt_vector_size(from_node->successors);
    for (size_t i = 0; i < succ_count; i++) {
        if ((cobalt_task_id_t)(uintptr_t)cobalt_vector_get_unchecked(from_node->successors, i) ==
            to_id) {
            return 0;
        }
    }
    if (cobalt_vector_push(from_node->successors, (void *)(uintptr_t)to_id) != 0) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    to_node->static_in_degree++;
    return 0;
}

int cobalt_taskgraph_compile(cobalt_taskgraph_t *graph)
{
    if (!graph) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }
    if (graph->compiled) {
        return 0;
    }

    size_t num_nodes = cobalt_vector_size(graph->nodes);
    if (num_nodes == 0) {
        graph->compiled = true;
        return 0;
    }

    size_t *in_degree = (size_t *)malloc(num_nodes * sizeof(size_t));
    size_t *queue     = (size_t *)malloc(num_nodes * sizeof(size_t));
    if (!in_degree || !queue) {
        free(in_degree);
        free(queue);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    size_t q_head = 0;
    size_t q_tail = 0;

    for (size_t i = 0; i < num_nodes; i++) {
        cobalt_task_node_t *node = (cobalt_task_node_t *)cobalt_vector_get(graph->nodes, i);
        in_degree[i]             = node->static_in_degree;
        if (in_degree[i] == 0) {
            queue[q_tail++] = i;
        }
    }

    size_t visited = 0;
    while (q_head < q_tail) {
        size_t              u      = queue[q_head++];
        cobalt_task_node_t *u_node = (cobalt_task_node_t *)cobalt_vector_get(graph->nodes, u);
        visited++;

        size_t succ_count = cobalt_vector_size(u_node->successors);
        for (size_t s = 0; s < succ_count; s++) {
            size_t v = (size_t)(uintptr_t)cobalt_vector_get(u_node->successors, s);
            in_degree[v]--;
            if (in_degree[v] == 0) {
                queue[q_tail++] = v;
            }
        }
    }

    free(in_degree);
    free(queue);

    if (visited != num_nodes) {
        /* Cycle detected! Graph is not a DAG. */
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }

    /* Cache all root nodes */
    cobalt_vector_destroy(graph->root_nodes);
    graph->root_nodes = cobalt_vector_create(8);
    for (size_t i = 0; i < num_nodes; i++) {
        cobalt_task_node_t *node = (cobalt_task_node_t *)cobalt_vector_get(graph->nodes, i);
        if (node->static_in_degree == 0) {
            cobalt_vector_push(graph->root_nodes, (void *)(uintptr_t)i);
        }
    }

    graph->compiled = true;
    return 0;
}

static void taskgraph_node_worker(void *arg)
{
    cobalt_task_node_t *node  = (cobalt_task_node_t *)arg;
    cobalt_taskgraph_t *graph = node->graph;

    /* Execute the task callback */
    node->fn(node->ctx);

    /* Decrement remaining in-degree for all successors */
    size_t succ_count = cobalt_vector_size(node->successors);
    for (size_t i = 0; i < succ_count; i++) {
        size_t              succ_id = (size_t)(uintptr_t)cobalt_vector_get(node->successors, i);
        cobalt_task_node_t *succ_node =
            (cobalt_task_node_t *)cobalt_vector_get(graph->nodes, succ_id);
        size_t prev =
            atomic_fetch_sub_explicit(&succ_node->remaining_in_degree, 1, memory_order_acq_rel);
        if (prev == 1) {
            /* All dependencies satisfied: submit to pool */
            if (cobalt_threadpool_submit(graph->bound_pool, taskgraph_node_worker, succ_node) !=
                0) {
                /* Synchronous fallback */
                taskgraph_node_worker(succ_node);
            }
        }
    }

    /* Signal completion on WaitGroup */
    if (graph->wg) {
        cobalt_waitgroup_done(graph->wg);
    }
}

int cobalt_taskgraph_run(cobalt_taskgraph_t *graph, cobalt_threadpool_t *pool)
{
    if (!graph || !pool || !graph->compiled) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }

    size_t num_nodes = cobalt_vector_size(graph->nodes);
    if (num_nodes == 0) {
        return 0;
    }

    /* Reset dynamic runtime counters */
    cobalt_taskgraph_reset(graph);

    /* Bind threadpool */
    graph->bound_pool = pool;

    /* Set up WaitGroup */
    if (graph->wg) {
        cobalt_waitgroup_destroy(graph->wg);
        graph->wg = NULL;
    }
    graph->wg = cobalt_waitgroup_create();
    if (!graph->wg) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }

    cobalt_waitgroup_add(graph->wg, (int64_t)num_nodes);

    /* Submit all root tasks (in-degree == 0) */
    size_t num_roots = cobalt_vector_size(graph->root_nodes);
    for (size_t i = 0; i < num_roots; i++) {
        size_t              root_id = (size_t)(uintptr_t)cobalt_vector_get(graph->root_nodes, i);
        cobalt_task_node_t *node = (cobalt_task_node_t *)cobalt_vector_get(graph->nodes, root_id);
        if (cobalt_threadpool_submit(pool, taskgraph_node_worker, node) != 0) {
            taskgraph_node_worker(node);
        }
    }

    return 0;
}

int cobalt_taskgraph_run_and_wait(cobalt_taskgraph_t *graph, cobalt_threadpool_t *pool)
{
    if (!graph || !pool || !graph->compiled) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return -1;
    }

    size_t num_nodes = cobalt_vector_size(graph->nodes);
    if (num_nodes == 0) {
        return 0;
    }

    if (cobalt_taskgraph_run(graph, pool) != 0) {
        return -1;
    }

    if (graph->wg) {
        cobalt_waitgroup_wait(graph->wg);
    }

    return 0;
}

void cobalt_taskgraph_reset(cobalt_taskgraph_t *graph)
{
    if (!graph) {
        return;
    }
    size_t n = cobalt_vector_size(graph->nodes);
    for (size_t i = 0; i < n; i++) {
        cobalt_task_node_t *node = (cobalt_task_node_t *)cobalt_vector_get(graph->nodes, i);
        if (node) {
            atomic_store_explicit(
                &node->remaining_in_degree, node->static_in_degree, memory_order_release);
        }
    }
}

size_t cobalt_taskgraph_node_count(const cobalt_taskgraph_t *graph)
{
    return graph ? cobalt_vector_size(graph->nodes) : 0;
}
