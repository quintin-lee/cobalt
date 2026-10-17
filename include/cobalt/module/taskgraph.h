#ifndef COBALT_MODULE_TASKGRAPH_H
#define COBALT_MODULE_TASKGRAPH_H

/**
 * @file taskgraph.h
 * @brief Reusable event-driven Directed Acyclic Graph (DAG) task scheduler
 * @details Implements DAG task orchestration featuring Kahn's algorithm cycle detection
 *          and atomic remaining in-degree push dispatch with zero-reallocation execution.
 *
 * @defgroup TaskGraph Task Graph
 * @ingroup Modules
 * @{
 */

#include "cobalt/memory/allocator.h"
#include "cobalt/module/threadpool.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t cobalt_task_id_t;
#define COBALT_INVALID_TASK_ID ((cobalt_task_id_t) - 1)

typedef struct cobalt_taskgraph cobalt_taskgraph_t;

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create a new empty TaskGraph using the system allocator
 * @return Newly allocated TaskGraph, or NULL on failure
 */
cobalt_taskgraph_t *cobalt_taskgraph_create(void);

/**
 * @brief Create a new TaskGraph using a custom memory allocator
 * @param alloc Custom allocator
 * @return Newly allocated TaskGraph, or NULL on failure
 */
cobalt_taskgraph_t *cobalt_taskgraph_create_with_alloc(cobalt_allocator_t *alloc);

/**
 * @brief Destroy a TaskGraph and release all graph data structures
 * @param graph TaskGraph instance. Safe with NULL.
 */
void cobalt_taskgraph_destroy(cobalt_taskgraph_t *graph);

/* -------------------------------------------------------------------------- */
/* Graph Topology Construction                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Add an executable task node to the graph
 * @param graph TaskGraph instance
 * @param fn Task callback function
 * @param ctx Context pointer passed to fn
 * @return Unique node ID, or COBALT_INVALID_TASK_ID on failure
 */
cobalt_task_id_t
cobalt_taskgraph_add_node(cobalt_taskgraph_t *graph, cobalt_task_fn_t fn, void *ctx);

/**
 * @brief Add a directed dependency edge: from_id must finish before to_id starts
 * @param graph TaskGraph instance
 * @param from_id Predecessor task node ID
 * @param to_id Successor task node ID
 * @return 0 on success, -1 on invalid node IDs or if graph is already compiled
 */
int cobalt_taskgraph_add_edge(cobalt_taskgraph_t *graph,
                              cobalt_task_id_t    from_id,
                              cobalt_task_id_t    to_id);

/**
 * @brief Validate, cycle-check, and compile the TaskGraph topology
 * @details Runs Kahn's topological sort algorithm to verify acyclicity.
 *          If a cycle is detected, returns -1 and sets COBALT_ERROR_INVALID_ARGUMENT.
 *          Extracts and caches root tasks (static in-degree == 0).
 * @param graph TaskGraph instance
 * @return 0 on success, -1 if cycle detected or invalid graph
 */
int cobalt_taskgraph_compile(cobalt_taskgraph_t *graph);

/* -------------------------------------------------------------------------- */
/* Execution & Reusable Dispatch                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Asynchronously start execution of the TaskGraph
 * @param graph Compiled TaskGraph instance
 * @param pool Thread pool for task execution
 * @return 0 on success, -1 if graph is not compiled or invalid
 */
int cobalt_taskgraph_run(cobalt_taskgraph_t *graph, cobalt_threadpool_t *pool);

/**
 * @brief Start TaskGraph execution and block until all nodes have completed
 * @details Uses Help-First WaitGroup so the calling thread assists in execution,
 *          eliminating deadlocks and reducing latency.
 * @param graph Compiled TaskGraph instance
 * @param pool Thread pool for task execution
 * @return 0 on success, -1 on failure
 */
int cobalt_taskgraph_run_and_wait(cobalt_taskgraph_t *graph, cobalt_threadpool_t *pool);

/**
 * @brief Reset dynamic runtime counters to allow graph re-execution
 * @param graph TaskGraph instance
 */
void cobalt_taskgraph_reset(cobalt_taskgraph_t *graph);

/**
 * @brief Get total number of task nodes in the graph
 * @param graph TaskGraph instance
 * @return Number of nodes, or 0 if NULL
 */
size_t cobalt_taskgraph_node_count(const cobalt_taskgraph_t *graph);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* COBALT_MODULE_TASKGRAPH_H */
