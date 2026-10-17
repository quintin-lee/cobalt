/**
 * @file taskgraph.c
 * @brief Reusable event-driven Directed Acyclic Graph (DAG) task scheduler
 */

#include "cobalt/module/taskgraph.h"

cobalt_taskgraph_t *cobalt_taskgraph_create(void)
{
    return NULL;
}

cobalt_taskgraph_t *cobalt_taskgraph_create_with_alloc(cobalt_allocator_t *alloc)
{
    (void)alloc;
    return NULL;
}

void cobalt_taskgraph_destroy(cobalt_taskgraph_t *graph)
{
    (void)graph;
}

cobalt_task_id_t
cobalt_taskgraph_add_node(cobalt_taskgraph_t *graph, cobalt_task_fn_t fn, void *ctx)
{
    (void)graph;
    (void)fn;
    (void)ctx;
    return COBALT_INVALID_TASK_ID;
}

int cobalt_taskgraph_add_edge(cobalt_taskgraph_t *graph,
                              cobalt_task_id_t    from_id,
                              cobalt_task_id_t    to_id)
{
    (void)graph;
    (void)from_id;
    (void)to_id;
    return -1;
}

int cobalt_taskgraph_compile(cobalt_taskgraph_t *graph)
{
    (void)graph;
    return -1;
}

int cobalt_taskgraph_run(cobalt_taskgraph_t *graph, cobalt_threadpool_t *pool)
{
    (void)graph;
    (void)pool;
    return -1;
}

int cobalt_taskgraph_run_and_wait(cobalt_taskgraph_t *graph, cobalt_threadpool_t *pool)
{
    (void)graph;
    (void)pool;
    return -1;
}

void cobalt_taskgraph_reset(cobalt_taskgraph_t *graph)
{
    (void)graph;
}

size_t cobalt_taskgraph_node_count(const cobalt_taskgraph_t *graph)
{
    (void)graph;
    return 0;
}
