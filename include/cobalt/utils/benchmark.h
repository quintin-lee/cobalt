#ifndef COBALT_UTILS_BENCHMARK_H
#define COBALT_UTILS_BENCHMARK_H

/**
 * @file benchmark.h
 * @brief Lightweight benchmarking utilities
 * @details Provides macros and helpers for micro-benchmarking Cobalt components.
 *          Uses CLOCK_MONOTONIC for sub-millisecond precision.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "B" {
#endif

/**
 * @brief Result of a single benchmark run
 */
typedef struct cobalt_bench_result {
    const char *name;       /**< Benchmark name */
    double      mean_ms;    /**< Mean elapsed time (ms) */
    double      stddev_ms;  /**< Standard deviation (ms) */
    double      min_ms;     /**< Minimum elapsed time (ms) */
    double      max_ms;     /**< Maximum elapsed time (ms) */
    size_t      iterations; /**< Number of iterations run */
} cobalt_bench_result_t;

/**
 * @brief Run a benchmark block N times and report statistics
 * @param name    Human-readable benchmark name
 * @param iters   Number of iterations to average over
 * @param block   Code block to benchmark (executed iters times)
 * @param result  Output result struct (may be NULL to skip collection)
 * @return mean elapsed time in milliseconds
 */
double cobalt_bench_run(const char *name,
                        size_t      iters,
                        void (*block)(void),
                        cobalt_bench_result_t *result);

/**
 * @brief Print a formatted benchmark results table
 * @param results Array of results (null-terminated by name == NULL)
 */
void cobalt_bench_print_results(const cobalt_bench_result_t *results);

/**
 * @brief Output benchmark results as JSON to a file
 * @param results Array of results (null-terminated by name == NULL)
 * @param fp      Output file pointer (use stdout for console)
 */
void cobalt_bench_output_json(const cobalt_bench_result_t *results, FILE *fp);

#ifdef __cplusplus
}
#endif

/**
 * @brief Benchmark macro: runs block iters times, prints summary
 * @param name    Benchmark name (string literal)
 * @param iters   Number of iterations
 * @param block   Code block to benchmark
 */
#define BENCH_RUN(name, iters, block)                                                               \
    do {                                                                                            \
        printf("  %-40s ", name);                                                                   \
        cobalt_bench_result_t _bench_result = {0};                                                  \
        double                _bench_mean   = cobalt_bench_run(name, iters, block, &_bench_result); \
        printf("%.3f ms  (mean: %.3f stddev: %.3f min: %.3f max: %.3f)\n",                          \
               _bench_mean,                                                                         \
               _bench_result.mean_ms,                                                               \
               _bench_result.stddev_ms,                                                             \
               _bench_result.min_ms,                                                                \
               _bench_result.max_ms);                                                               \
    } while (0)

#endif /* COBALT_UTILS_BENCHMARK_H */
