/**
 * @file parallel_sort.c
 * @brief High-performance multi-core parallel sorting algorithms
 */

#include "cobalt/module/parallel_sort.h"
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Parallel PDQSort Implementation                                            */
/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */
/* Parallel Stable TimSort Implementation                                     */
/* -------------------------------------------------------------------------- */

typedef struct {
    char               *base;
    size_t              nmemb;
    size_t              size;
    compare_func_t      compar;
    cobalt_allocator_t *alloc;
    size_t              num_chunks;
} chunk_sort_ctx_t;

static void chunk_sort_worker(size_t chunk_idx, void *arg)
{
    chunk_sort_ctx_t *ctx   = (chunk_sort_ctx_t *)arg;
    size_t            start = chunk_idx * ctx->nmemb / ctx->num_chunks;
    size_t            end   = (chunk_idx + 1) * ctx->nmemb / ctx->num_chunks;
    size_t            len   = end - start;
    if (len > 1) {
        cobalt_stable_sort_with_alloc(
            ctx->base + start * ctx->size, len, ctx->size, ctx->compar, ctx->alloc);
    }
}

typedef struct {
    char               *base;
    char               *scratch;
    size_t              size;
    compare_func_t      compar;
    size_t              start1;
    size_t              mid;
    size_t              end2;
    cobalt_waitgroup_t *wg;
} merge_task_t;

static void merge_task_worker(void *arg)
{
    merge_task_t  *task   = (merge_task_t *)arg;
    size_t         lenA   = task->mid - task->start1;
    size_t         lenB   = task->end2 - task->mid;
    char          *dest   = task->scratch + task->start1 * task->size;
    char          *srcA   = task->base + task->start1 * task->size;
    char          *srcB   = task->base + task->mid * task->size;
    size_t         size   = task->size;
    compare_func_t compar = task->compar;

    size_t idxA = 0;
    size_t idxB = 0;
    size_t out  = 0;

    /* Enforce strict stability: pick left item when compar(a, b) <= 0 */
    while (idxA < lenA && idxB < lenB) {
        if (compar(srcA + idxA * size, srcB + idxB * size) <= 0) {
            memcpy(dest + out * size, srcA + idxA * size, size);
            idxA++;
        } else {
            memcpy(dest + out * size, srcB + idxB * size, size);
            idxB++;
        }
        out++;
    }

    if (idxA < lenA) {
        memcpy(dest + out * size, srcA + idxA * size, (lenA - idxA) * size);
    }
    if (idxB < lenB) {
        memcpy(dest + out * size, srcB + idxB * size, (lenB - idxB) * size);
    }

    /* Copy merged slice back to source array */
    memcpy(srcA, dest, (lenA + lenB) * size);

    cobalt_waitgroup_done(task->wg);
    free(task);
}

void cobalt_parallel_stable_sort_with_alloc(cobalt_threadpool_t *pool,
                                            void                *base,
                                            size_t               nmemb,
                                            size_t               size,
                                            compare_func_t       compar,
                                            cobalt_allocator_t  *alloc)
{
    if (!base || nmemb <= 1 || size == 0 || !compar) {
        return;
    }

    size_t num_threads = pool ? cobalt_threadpool_thread_count(pool) : 0;
    if (num_threads <= 1 || nmemb <= 4096) {
        cobalt_stable_sort_with_alloc(base, nmemb, size, compar, alloc);
        return;
    }

    /* Choose chunk count K as a power of 2 */
    size_t K = 1;
    while (K < num_threads * 2 && K < 64) {
        K *= 2;
    }
    if (K < 2) {
        K = 2;
    }
    if (nmemb / K < 1024) {
        K = 1;
        while (K * 2 <= nmemb / 1024 && K < 64) {
            K *= 2;
        }
    }
    if (K <= 1) {
        cobalt_stable_sort_with_alloc(base, nmemb, size, compar, alloc);
        return;
    }

    /* Allocate full scratch buffer */
    char *scratch = NULL;
    if (alloc && alloc->alloc) {
        scratch = (char *)alloc->alloc(alloc, nmemb * size);
    } else {
        scratch = (char *)malloc(nmemb * size);
    }

    if (!scratch) {
        cobalt_stable_sort_with_alloc(base, nmemb, size, compar, alloc);
        return;
    }

    /* Phase 1: Parallel sorting of individual chunks */
    chunk_sort_ctx_t sort_ctx = {
        .base       = (char *)base,
        .nmemb      = nmemb,
        .size       = size,
        .compar     = compar,
        .alloc      = alloc,
        .num_chunks = K,
    };
    cobalt_threadpool_parallel_for(pool, 0, K, 1, chunk_sort_worker, &sort_ctx);

    /* Phase 2: Hierarchical binary tree merge */
    for (size_t step = 1; step < K; step *= 2) {
        cobalt_waitgroup_t *level_wg = cobalt_waitgroup_create();
        if (!level_wg) {
            break;
        }

        for (size_t i = 0; i < K; i += 2 * step) {
            if (i + step < K) {
                size_t start1 = i * nmemb / K;
                size_t mid    = (i + step) * nmemb / K;
                size_t end2   = (i + 2 * step < K) ? ((i + 2 * step) * nmemb / K) : nmemb;

                merge_task_t *task = (merge_task_t *)malloc(sizeof(merge_task_t));
                if (task) {
                    task->base    = (char *)base;
                    task->scratch = scratch;
                    task->size    = size;
                    task->compar  = compar;
                    task->start1  = start1;
                    task->mid     = mid;
                    task->end2    = end2;
                    task->wg      = level_wg;

                    cobalt_waitgroup_add(level_wg, 1);
                    if (cobalt_threadpool_submit(pool, merge_task_worker, task) != 0) {
                        cobalt_waitgroup_done(level_wg);
                        free(task);
                        /* Synchronous fallback for this pair */
                        size_t lenA = mid - start1;
                        size_t lenB = end2 - mid;
                        char  *dest = scratch + start1 * size;
                        char  *srcA = (char *)base + start1 * size;
                        char  *srcB = (char *)base + mid * size;
                        size_t idxA = 0;
                        size_t idxB = 0;
                        size_t out  = 0;
                        while (idxA < lenA && idxB < lenB) {
                            if (compar(srcA + idxA * size, srcB + idxB * size) <= 0) {
                                memcpy(dest + out * size, srcA + idxA * size, size);
                                idxA++;
                            } else {
                                memcpy(dest + out * size, srcB + idxB * size, size);
                                idxB++;
                            }
                            out++;
                        }
                        if (idxA < lenA) {
                            memcpy(dest + out * size, srcA + idxA * size, (lenA - idxA) * size);
                        }
                        if (idxB < lenB) {
                            memcpy(dest + out * size, srcB + idxB * size, (lenB - idxB) * size);
                        }
                        memcpy(srcA, dest, (lenA + lenB) * size);
                    }
                }
            }
        }

        cobalt_waitgroup_wait(level_wg);
        cobalt_waitgroup_destroy(level_wg);
    }

    if (alloc && alloc->free) {
        alloc->free(alloc, scratch);
    } else {
        free(scratch);
    }
}

void cobalt_parallel_stable_sort(
    cobalt_threadpool_t *pool, void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    cobalt_parallel_stable_sort_with_alloc(pool, base, nmemb, size, compar, NULL);
}
