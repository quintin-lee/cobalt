/**
 * @file parallel_sort.c
 * @brief High-performance multi-core parallel sorting algorithms
 */

#include "cobalt/module/parallel_sort.h"
#include <string.h>

void cobalt_parallel_qsort(
    cobalt_threadpool_t *pool, void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    (void)pool;
    (void)base;
    (void)nmemb;
    (void)size;
    (void)compar;
}

void cobalt_parallel_qsort_with_cutoff(cobalt_threadpool_t *pool,
                                       void                *base,
                                       size_t               nmemb,
                                       size_t               size,
                                       compare_func_t       compar,
                                       size_t               cutoff)
{
    (void)pool;
    (void)base;
    (void)nmemb;
    (void)size;
    (void)compar;
    (void)cutoff;
}

void cobalt_parallel_stable_sort(
    cobalt_threadpool_t *pool, void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    (void)pool;
    (void)base;
    (void)nmemb;
    (void)size;
    (void)compar;
}

void cobalt_parallel_stable_sort_with_alloc(cobalt_threadpool_t *pool,
                                            void                *base,
                                            size_t               nmemb,
                                            size_t               size,
                                            compare_func_t       compar,
                                            cobalt_allocator_t  *alloc)
{
    (void)pool;
    (void)base;
    (void)nmemb;
    (void)size;
    (void)compar;
    (void)alloc;
}
