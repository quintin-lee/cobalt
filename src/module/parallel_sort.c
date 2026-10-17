/**
 * @file parallel_sort.c
 * @brief High-performance multi-core parallel sorting algorithms
 */

#include "cobalt/module/parallel_sort.h"
#include <stdlib.h>
#include <string.h>

typedef struct parallel_qsort_task {
    cobalt_threadpool_t *pool;
    char                *base;
    size_t               nmemb;
    size_t               size;
    compare_func_t       compar;
    size_t               cutoff;
    cobalt_waitgroup_t  *wg;
} parallel_qsort_task_t;

static void parallel_qsort_impl(cobalt_threadpool_t *pool,
                                char                *base,
                                size_t               nmemb,
                                size_t               size,
                                compare_func_t       compar,
                                size_t               cutoff,
                                cobalt_waitgroup_t  *wg);

static void parallel_qsort_worker(void *arg)
{
    parallel_qsort_task_t *task_ptr = (parallel_qsort_task_t *)arg;
    parallel_qsort_task_t  task     = *task_ptr;
    free(task_ptr);

    parallel_qsort_impl(
        task.pool, task.base, task.nmemb, task.size, task.compar, task.cutoff, task.wg);

    cobalt_waitgroup_done(task.wg);
}

static void parallel_qsort_impl(cobalt_threadpool_t *pool,
                                char                *base,
                                size_t               nmemb,
                                size_t               size,
                                compare_func_t       compar,
                                size_t               cutoff,
                                cobalt_waitgroup_t  *wg)
{
    while (nmemb > cutoff) {
        char       *first = base;
        char       *mid   = base + (nmemb / 2) * size;
        char       *last  = base + (nmemb - 1) * size;
        const void *pivot = first;

        if (compar(first, mid) < 0) {
            if (compar(mid, last) < 0) {
                pivot = mid;
            } else if (compar(first, last) < 0) {
                pivot = last;
            } else {
                pivot = first;
            }
        } else {
            if (compar(first, last) < 0) {
                pivot = first;
            } else if (compar(mid, last) < 0) {
                pivot = last;
            } else {
                pivot = mid;
            }
        }

        char  pivot_buf[256];
        void *pivot_val = NULL;
        if (size <= sizeof(pivot_buf)) {
            memcpy(pivot_buf, pivot, size);
            pivot_val = pivot_buf;
        } else {
            pivot_val = malloc(size);
            if (pivot_val) {
                memcpy(pivot_val, pivot, size);
            } else {
                pivot_val = (void *)pivot;
            }
        }

        size_t split = cobalt_partition(base, nmemb, size, pivot_val, compar);

        if (pivot_val && pivot_val != pivot_buf && pivot_val != pivot) {
            free(pivot_val);
        }

        if (split == 0 || split == nmemb) {
            cobalt_qsort(base, nmemb, size, compar);
            return;
        }

        char  *left_base   = base;
        size_t left_nmemb  = split;
        char  *right_base  = base + split * size;
        size_t right_nmemb = nmemb - split;

        char  *spawn_base  = left_base;
        size_t spawn_nmemb = left_nmemb;
        char  *loop_base   = right_base;
        size_t loop_nmemb  = right_nmemb;

        if (spawn_nmemb < loop_nmemb) {
            spawn_base  = right_base;
            spawn_nmemb = right_nmemb;
            loop_base   = left_base;
            loop_nmemb  = left_nmemb;
        }

        if (spawn_nmemb > cutoff) {
            parallel_qsort_task_t *task =
                (parallel_qsort_task_t *)malloc(sizeof(parallel_qsort_task_t));
            if (task) {
                task->pool   = pool;
                task->base   = spawn_base;
                task->nmemb  = spawn_nmemb;
                task->size   = size;
                task->compar = compar;
                task->cutoff = cutoff;
                task->wg     = wg;

                cobalt_waitgroup_add(wg, 1);
                if (cobalt_threadpool_submit(pool, parallel_qsort_worker, task) != 0) {
                    cobalt_waitgroup_done(wg);
                    free(task);
                    parallel_qsort_impl(pool, spawn_base, spawn_nmemb, size, compar, cutoff, wg);
                }
            } else {
                parallel_qsort_impl(pool, spawn_base, spawn_nmemb, size, compar, cutoff, wg);
            }
        } else {
            cobalt_qsort(spawn_base, spawn_nmemb, size, compar);
        }

        base  = loop_base;
        nmemb = loop_nmemb;
    }

    if (nmemb > 1) {
        cobalt_qsort(base, nmemb, size, compar);
    }
}

void cobalt_parallel_qsort_with_cutoff(cobalt_threadpool_t *pool,
                                       void                *base,
                                       size_t               nmemb,
                                       size_t               size,
                                       compare_func_t       compar,
                                       size_t               cutoff)
{
    if (!base || nmemb <= 1 || size == 0 || !compar) {
        return;
    }

    size_t num_threads = pool ? cobalt_threadpool_thread_count(pool) : 0;
    if (num_threads <= 1 || nmemb <= cutoff) {
        cobalt_qsort(base, nmemb, size, compar);
        return;
    }

    cobalt_waitgroup_t *wg = cobalt_waitgroup_create();
    if (!wg) {
        cobalt_qsort(base, nmemb, size, compar);
        return;
    }

    parallel_qsort_impl(pool, (char *)base, nmemb, size, compar, cutoff, wg);

    cobalt_waitgroup_wait(wg);
    cobalt_waitgroup_destroy(wg);
}

void cobalt_parallel_qsort(
    cobalt_threadpool_t *pool, void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    if (!base || nmemb <= 1 || size == 0 || !compar) {
        return;
    }

    size_t num_threads = pool ? cobalt_threadpool_thread_count(pool) : 1;
    if (num_threads == 0) {
        num_threads = 1;
    }

    size_t cutoff = nmemb / (4 * num_threads);
    if (cutoff < 4096) {
        cutoff = 4096;
    }

    cobalt_parallel_qsort_with_cutoff(pool, base, nmemb, size, compar, cutoff);
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
