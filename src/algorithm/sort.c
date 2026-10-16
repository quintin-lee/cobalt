/**
 * @file sort.c
 * @brief Implementation of general sorting algorithms
 */
#include "cobalt/algorithm/sort.h"
#include "cobalt/container/list.h"
#include "cobalt/memory/allocator.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/* compare_func_t is already defined in sort.h */

/* -------------------------------------------------------------------------- */
/* Internal helpers with explicit allocator                                   */
/* -------------------------------------------------------------------------- */

static inline void cobalt_swap_bytes(char *a, char *b, size_t size)
{
    if (a == b) {
        return;
    }
    if ((((uintptr_t)a | (uintptr_t)b | size) & (sizeof(uint64_t) - 1)) == 0) {
        uint64_t *p1 = (uint64_t *)a;
        uint64_t *p2 = (uint64_t *)b;
        size_t    n  = size / sizeof(uint64_t);
        while (n--) {
            uint64_t tmp = *p1;
            *p1++        = *p2;
            *p2++        = tmp;
        }
    } else if ((((uintptr_t)a | (uintptr_t)b | size) & (sizeof(uint32_t) - 1)) == 0) {
        uint32_t *p1 = (uint32_t *)a;
        uint32_t *p2 = (uint32_t *)b;
        size_t    n  = size / sizeof(uint32_t);
        while (n--) {
            uint32_t tmp = *p1;
            *p1++        = *p2;
            *p2++        = tmp;
        }
    } else {
        while (size--) {
            char tmp = *a;
            *a++     = *b;
            *b++     = tmp;
        }
    }
}

static void
heapsort_sift_down(char *arr, size_t root, size_t count, size_t size, compare_func_t compar)
{
    while (root < count / 2) {
        size_t child = root * 2 + 1;
        if (child + 1 < count && compar(arr + (child + 1) * size, arr + child * size) > 0) {
            child++;
        }
        if (compar(arr + child * size, arr + root * size) > 0) {
            cobalt_swap_bytes(arr + root * size, arr + child * size, size);
            root = child;
        } else {
            break;
        }
    }
}

static void cobalt_heapsort_impl(char *arr, size_t nmemb, size_t size, compare_func_t compar)
{
    if (nmemb <= 1) {
        return;
    }
    for (size_t i = nmemb / 2; i > 0; i--) {
        heapsort_sift_down(arr, i - 1, nmemb, size, compar);
    }
    for (size_t i = nmemb - 1; i > 0; i--) {
        cobalt_swap_bytes(arr, arr + i * size, size);
        heapsort_sift_down(arr, 0, i, size, compar);
    }
}

void cobalt_heapsort(void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    if (!base || nmemb <= 1 || size == 0 || !compar) {
        return;
    }
    cobalt_heapsort_impl((char *)base, nmemb, size, compar);
}

static void insertion_sort_impl(
    void *base, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc)
{
    if (!base || nmemb <= 1 || !compar || size == 0) {
        return;
    }
    char                      *arr = (char *)base;
    _Alignas(max_align_t) char stack_buf[128];
    char                      *key = NULL;

    if (alloc) {
        key = (char *)alloc->alloc(alloc, size);
        if (!key) {
            return;
        }
    } else {
        if (size <= sizeof(stack_buf)) {
            key = stack_buf;
        } else {
            key = (char *)malloc(size);
            if (!key) {
                return;
            }
        }
    }

    for (size_t i = 1; i < nmemb; i++) {
        memcpy(key, arr + i * size, size);
        size_t j = i;
        while (j > 0 && compar(arr + (j - 1) * size, key) > 0) {
            memcpy(arr + j * size, arr + (j - 1) * size, size);
            j--;
        }
        if (j != i) {
            memcpy(arr + j * size, key, size);
        }
    }

    if (alloc) {
        alloc->free(alloc, key);
    } else if (key != stack_buf) {
        free(key);
    }
}

static size_t partition_impl(void               *base,
                             size_t              nmemb,
                             size_t              size,
                             const void         *pivot,
                             compare_func_t      compar,
                             cobalt_allocator_t *alloc)
{
    if (!base || nmemb == 0 || size == 0 || !compar || !pivot) {
        return 0;
    }

    void *track_buf = NULL;
    if (alloc) {
        track_buf = alloc->alloc(alloc, size);
        if (!track_buf) {
            return 0;
        }
    }

    char  *arr   = (char *)base;
    size_t left  = 0;
    size_t right = nmemb;

    while (left < right) {
        if (compar(arr + left * size, pivot) < 0) {
            left++;
        } else {
            right--;
            if (left != right) {
                cobalt_swap_bytes(arr + left * size, arr + right * size, size);
            }
        }
    }

    if (alloc) {
        alloc->free(alloc, track_buf);
    }
    return left;
}

/* -------------------------------------------------------------------------- */
/* PDQSort (Pattern-Defeating Quicksort)                                      */
/* -------------------------------------------------------------------------- */

static inline size_t pdq_log2(size_t n)
{
    size_t log = 0;
    while (n >>= 1) {
        log++;
    }
    return log;
}

static size_t
pdq_median3(const char *arr, size_t a, size_t b, size_t c, size_t size, compare_func_t compar)
{
    const char *pa = arr + a * size;
    const char *pb = arr + b * size;
    const char *pc = arr + c * size;

    if (compar(pa, pb) < 0) {
        if (compar(pb, pc) < 0) {
            return b;
        }
        return compar(pa, pc) < 0 ? c : a;
    }
    if (compar(pa, pc) < 0) {
        return a;
    }
    return compar(pb, pc) < 0 ? c : b;
}

static size_t pdq_ninther(const char *arr, size_t nmemb, size_t size, compare_func_t compar)
{
    size_t s   = nmemb / 8;
    size_t mid = nmemb / 2;
    size_t m1  = pdq_median3(arr, 0, s, 2 * s, size, compar);
    size_t m2  = pdq_median3(arr, mid - s, mid, mid + s, size, compar);
    size_t m3  = pdq_median3(arr, nmemb - 1 - 2 * s, nmemb - 1 - s, nmemb - 1, size, compar);
    return pdq_median3(arr, m1, m2, m3, size, compar);
}

static void
pdqsort_loop(char *arr, size_t nmemb, size_t size, compare_func_t compar, int bad_allowed)
{
    while (nmemb > 24) {
        if (bad_allowed <= 0) {
            cobalt_heapsort_impl(arr, nmemb, size, compar);
            return;
        }

        size_t pivot_idx;
        if (nmemb > 128) {
            pivot_idx = pdq_ninther(arr, nmemb, size, compar);
        } else {
            pivot_idx = pdq_median3(arr, 0, nmemb / 2, nmemb - 1, size, compar);
        }

        if (pivot_idx != 0) {
            cobalt_swap_bytes(arr, arr + pivot_idx * size, size);
        }

        size_t lt = 0;
        size_t i  = 1;
        size_t gt = nmemb - 1;

        while (i <= gt) {
            int cmp = compar(arr + i * size, arr + lt * size);
            if (cmp < 0) {
                cobalt_swap_bytes(arr + lt * size, arr + i * size, size);
                lt++;
                i++;
            } else if (cmp > 0) {
                cobalt_swap_bytes(arr + i * size, arr + gt * size, size);
                gt--;
            } else {
                i++;
            }
        }

        size_t left_size  = lt;
        size_t right_size = nmemb - (gt + 1);

        if (left_size < nmemb / 8 || right_size < nmemb / 8) {
            bad_allowed--;
        }

        if (left_size < right_size) {
            if (left_size > 1) {
                pdqsort_loop(arr, left_size, size, compar, bad_allowed);
            }
            arr   = arr + (gt + 1) * size;
            nmemb = right_size;
        } else {
            if (right_size > 1) {
                pdqsort_loop(arr + (gt + 1) * size, right_size, size, compar, bad_allowed);
            }
            nmemb = left_size;
        }
    }

    insertion_sort_impl(arr, nmemb, size, compar, NULL);
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Perform quicksort via pattern-defeating quicksort (PDQSort)
 *
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparator establishing the sort order
 * @note Unstable: equal elements may change relative order
 */
void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    if (!base || nmemb <= 1 || size == 0 || !compar) {
        return;
    }
    int bad_allowed = (int)(2 * pdq_log2(nmemb));
    pdqsort_loop((char *)base, nmemb, size, compar, bad_allowed);
}

/**
 * @brief Perform insertion sort
 *
 * @details Iterates through the elements, inserting each into its correct position in the
 *          already sorted sub-sequence. Efficient for small or nearly sorted arrays.
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparator establishing the sort order
 * @note Requires a temporary element-size buffer; silently skips sorting when allocation fails
 */
void cobalt_insertion_sort(void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    insertion_sort_impl(base, nmemb, size, compar, NULL);
}

/**
 * @brief Perform insertion sort using a custom allocator
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparator establishing the sort order
 * @param alloc Custom allocator (NULL falls back to system); caller must not free base
 * @note Same behavior as cobalt_insertion_sort but with injectable allocator
 */
void cobalt_insertion_sort_with_alloc(
    void *base, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc)
{
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }
    insertion_sort_impl(base, nmemb, size, compar, alloc);
}

/**
 * @brief Sort a linked list using merge sort (stable, O(n log n))
 * @details Delegates to cobalt_list_merge_sort() in list.c
 *
 * @param head Pointer to the list head pointer (updated in place)
 * @param count Optional node count used for optimization, may be NULL
 * @param compar Comparator establishing the sort order
 */
void cobalt_list_sort(void **head, size_t *count, compare_func_t compar)
{
    if (!head || !compar) {
        return;
    }
    cobalt_list_node_t **list_head = (cobalt_list_node_t **)head;
    cobalt_list_merge_sort(list_head, count, compar);
}

/* -------------------------------------------------------------------------- */
/* Adaptive Run MergeSort (TimSort-lite)                                      */
/* -------------------------------------------------------------------------- */

typedef struct {
    size_t start;
    size_t len;
} run_t;

static void reverse_run(char *arr, size_t len, size_t size)
{
    if (len <= 1) {
        return;
    }
    size_t left  = 0;
    size_t right = len - 1;
    while (left < right) {
        cobalt_swap_bytes(arr + left * size, arr + right * size, size);
        left++;
        right--;
    }
}

static void merge_runs(char          *arr,
                       size_t         a_start,
                       size_t         a_len,
                       size_t         b_len,
                       size_t         size,
                       compare_func_t compar,
                       char          *scratch)
{
    char *a = arr + a_start * size;
    char *b = arr + (a_start + a_len) * size;

    if (a_len <= b_len) {
        /* Copy left run A to scratch buffer and merge forward */
        memcpy(scratch, a, a_len * size);

        size_t i    = 0;
        size_t j    = 0;
        size_t dest = 0;

        while (i < a_len && j < b_len) {
            /* Stability: prefer scratch (left run A) when equal */
            if (compar(scratch + i * size, b + j * size) <= 0) {
                memcpy(a + dest * size, scratch + i * size, size);
                i++;
            } else {
                memcpy(a + dest * size, b + j * size, size);
                j++;
            }
            dest++;
        }

        if (i < a_len) {
            memcpy(a + dest * size, scratch + i * size, (a_len - i) * size);
        }
    } else {
        /* When left run A is larger than right run B, copy run B to scratch
         * and merge backward to guarantee scratch never exceeds (nmemb + 1) / 2.
         * Stability: when elements are equal, copy from scratch (run B) first
         * into higher index so run A elements remain before run B elements. */
        memcpy(scratch, b, b_len * size);

        size_t i    = a_len;
        size_t j    = b_len;
        size_t dest = a_len + b_len;

        while (i > 0 && j > 0) {
            dest--;
            if (compar(a + (i - 1) * size, scratch + (j - 1) * size) > 0) {
                i--;
                memcpy(a + dest * size, a + i * size, size);
            } else {
                j--;
                memcpy(a + dest * size, scratch + j * size, size);
            }
        }

        if (j > 0) {
            memcpy(a, scratch, j * size);
        }
    }
}

static void adaptive_merge_sort(
    char *arr, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc)
{
    if (nmemb <= 24) {
        insertion_sort_impl(arr, nmemb, size, compar, alloc);
        return;
    }

    /* Allocate scratch buffer of size ((nmemb / 2) + (nmemb & 1)) * size */
    size_t max_run_len = (nmemb / 2) + (nmemb & 1);
    char  *scratch     = NULL;
    if (alloc) {
        scratch = (char *)alloc->alloc(alloc, max_run_len * size);
    } else {
        scratch = (char *)malloc(max_run_len * size);
    }

    if (!scratch) {
        /* OOM fallback gracefully to insertion_sort_impl */
        insertion_sort_impl(arr, nmemb, size, compar, alloc);
        return;
    }

    /* Identify natural runs */
    run_t  stack[64];
    size_t stack_size = 0;
    size_t curr       = 0;

    while (curr < nmemb) {
        size_t run_len = 1;
        if (curr + 1 < nmemb) {
            if (compar(arr + curr * size, arr + (curr + 1) * size) > 0) {
                /* Strictly descending run: reverse with reverse_run */
                run_len = 2;
                while (curr + run_len < nmemb && compar(arr + (curr + run_len - 1) * size,
                                                        arr + (curr + run_len) * size) > 0) {
                    run_len++;
                }
                reverse_run(arr + curr * size, run_len, size);
            } else {
                /* Non-descending run */
                run_len = 2;
                while (curr + run_len < nmemb && compar(arr + (curr + run_len - 1) * size,
                                                        arr + (curr + run_len) * size) <= 0) {
                    run_len++;
                }
            }
        }

        /* Short runs (< 16) are extended up to min(16, remaining) */
        size_t minrun = 16;
        if (run_len < minrun && curr + run_len < nmemb) {
            size_t force_len = nmemb - curr;
            if (force_len > minrun) {
                force_len = minrun;
            }
            insertion_sort_impl(arr + curr * size, force_len, size, compar, alloc);
            run_len = force_len;
        }

        while (stack_size >= 64) {
            size_t n = stack_size - 1;
            merge_runs(
                arr, stack[n - 1].start, stack[n - 1].len, stack[n].len, size, compar, scratch);
            stack[n - 1].len += stack[n].len;
            stack_size--;
        }

        if (stack_size < 64) {
            stack[stack_size].start = curr;
            stack[stack_size].len   = run_len;
            stack_size++;
        }
        curr += run_len;

        /* Merge balance */
        while (stack_size >= 2) {
            size_t n = stack_size - 1;
            if (stack_size >= 3 && stack[n - 2].len <= stack[n - 1].len + stack[n].len) {
                if (stack[n - 2].len < stack[n].len) {
                    merge_runs(arr,
                               stack[n - 2].start,
                               stack[n - 2].len,
                               stack[n - 1].len,
                               size,
                               compar,
                               scratch);
                    stack[n - 2].len += stack[n - 1].len;
                    stack[n - 1] = stack[n];
                    stack_size--;
                } else {
                    merge_runs(arr,
                               stack[n - 1].start,
                               stack[n - 1].len,
                               stack[n].len,
                               size,
                               compar,
                               scratch);
                    stack[n - 1].len += stack[n].len;
                    stack_size--;
                }
            } else if (stack[n - 1].len <= stack[n].len) {
                merge_runs(
                    arr, stack[n - 1].start, stack[n - 1].len, stack[n].len, size, compar, scratch);
                stack[n - 1].len += stack[n].len;
                stack_size--;
            } else {
                break;
            }
        }
    }

    /* Unwind all remaining runs */
    while (stack_size > 1) {
        size_t n = stack_size - 1;
        if (stack_size >= 3 && stack[n - 2].len < stack[n].len) {
            merge_runs(
                arr, stack[n - 2].start, stack[n - 2].len, stack[n - 1].len, size, compar, scratch);
            stack[n - 2].len += stack[n - 1].len;
            stack[n - 1] = stack[n];
            stack_size--;
        } else {
            merge_runs(
                arr, stack[n - 1].start, stack[n - 1].len, stack[n].len, size, compar, scratch);
            stack[n - 1].len += stack[n].len;
            stack_size--;
        }
    }

    if (alloc) {
        alloc->free(alloc, scratch);
    } else {
        free(scratch);
    }
}

/**
 * @brief Perform a stable sort via adaptive run mergesort (TimSort-lite)
 *
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparator establishing the sort order
 * @note Stable: equal elements preserve their relative order
 */
void cobalt_stable_sort(void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    if (!base || nmemb <= 1 || size == 0 || !compar) {
        return;
    }
    adaptive_merge_sort((char *)base, nmemb, size, compar, NULL);
}

/**
 * @brief Perform a stable sort using a custom allocator
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param compar Comparator establishing the sort order
 * @param alloc Custom allocator (NULL falls back to system)
 * @note Same behavior as cobalt_stable_sort but with injectable allocator
 */
void cobalt_stable_sort_with_alloc(
    void *base, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc)
{
    if (!base || nmemb <= 1 || size == 0 || !compar) {
        return;
    }
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }
    adaptive_merge_sort((char *)base, nmemb, size, compar, alloc);
}

/**
 * @brief Partition an array around a pivot value (Lomuto-style)
 *
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param pivot Pivot value that elements are compared against
 * @param compar Comparator establishing the element order
 * @return Index of the first element in the greater-or-equal partition, 0 on invalid input
 * @note Zero dynamic allocations; elements swapped in place
 */
size_t
cobalt_partition(void *base, size_t nmemb, size_t size, const void *pivot, compare_func_t compar)
{
    return partition_impl(base, nmemb, size, pivot, compar, NULL);
}

/**
 * @brief Partition an array around a pivot using a custom allocator
 * @param base Pointer to the first array element
 * @param nmemb Number of elements in the array
 * @param size Size of each element in bytes
 * @param pivot Pivot value that elements are compared against
 * @param compar Comparator establishing the element order
 * @param alloc Custom allocator (NULL falls back to system)
 * @return Index of the first element in the greater-or-equal partition, 0 on invalid input
 * @note Same behavior as cobalt_partition but with injectable allocator
 */
size_t cobalt_partition_with_alloc(void               *base,
                                   size_t              nmemb,
                                   size_t              size,
                                   const void         *pivot,
                                   compare_func_t      compar,
                                   cobalt_allocator_t *alloc)
{
    return partition_impl(base, nmemb, size, pivot, compar, alloc);
}

/**
 * @brief Remove consecutive duplicates from a sorted array in place
 *
 * @param base Pointer to the first array element
 * @param nmemb On entry the input count, on exit the deduplicated count
 * @param size Size of each element in bytes
 * @param compar Comparator establishing element equality
 * @return New logical size after deduplication
 */
size_t cobalt_unique(void *base, size_t *nmemb, size_t size, compare_func_t compar)
{
    if (!base || !nmemb || *nmemb <= 1 || !compar) {
        return nmemb ? *nmemb : 0;
    }
    size_t count = *nmemb;
    size_t write = 1;
    char  *arr   = (char *)base;
    for (size_t i = 1; i < count; i++) {
        if (compar(arr + i * size, arr + (i - 1) * size) != 0) {
            if (write != i) {
                memcpy(arr + write * size, arr + i * size, size);
            }
            write++;
        }
    }
    *nmemb = write;
    return write;
}
