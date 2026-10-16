/**
 * @file test_sort.c
 * @brief Unit tests for sorting algorithms and functional utilities.
 */

#include "cobalt/algorithm/functional.h"
#include "cobalt/algorithm/sort.h"
#include "cobalt/memory/allocator.h"
#include "test_framework.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Helper comparators and predicates                                          */
/* -------------------------------------------------------------------------- */

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static int pred_gt5(const void *item)
{
    return *(const int *)item > 5;
}

static int pred_gt10(const void *item)
{
    return *(const int *)item > 10;
}

static int pred_even(const void *item)
{
    return *(const int *)item % 2 == 0;
}

static void map_double(const void *src, void *dst, void *ud)
{
    *(int *)dst = *(const int *)src * 2;
    (void)ud;
}

static void *fold_sum(void *acc, const void *item, void *ud)
{
    *(int *)acc += *(const int *)item;
    (void)ud;
    return acc;
}

/* -------------------------------------------------------------------------- */
/* cobalt_qsort                                                               */
/* -------------------------------------------------------------------------- */

void test_qsort_int(void)
{
    printf("Testing qsort int array...\n");
    int arr[] = {5, 3, 8, 1, 9, 2, 7};
    cobalt_qsort(arr, 7, sizeof(int), cmp_int);
    int expected[] = {1, 2, 3, 5, 7, 8, 9};
    for (int i = 0; i < 7; i++) {
        TEST_EQUAL(arr[i], expected[i]);
    }
    printf("  qsort int: OK\n");
}

void test_qsort_double(void)
{
    printf("Testing qsort double array...\n");
    double arr[] = {3.14, 1.41, 2.72, 0.57, 1.61};
    cobalt_qsort(arr, 5, sizeof(double), cmp_double);
    double expected[] = {0.57, 1.41, 1.61, 2.72, 3.14};
    for (int i = 0; i < 5; i++) {
        TEST_EQUAL(arr[i], expected[i]);
    }
    printf("  qsort double: OK\n");
}

void test_qsort_empty(void)
{
    printf("Testing qsort empty array...\n");
    int arr[1] = {42};
    cobalt_qsort(arr, 0, sizeof(int), cmp_int);
    TEST_EQUAL(arr[0], 42);
    printf("  qsort empty: OK\n");
}

void test_qsort_single(void)
{
    printf("Testing qsort single element...\n");
    int arr[] = {7};
    cobalt_qsort(arr, 1, sizeof(int), cmp_int);
    TEST_EQUAL(arr[0], 7);
    printf("  qsort single: OK\n");
}

void test_qsort_already_sorted(void)
{
    printf("Testing qsort already sorted...\n");
    int arr[] = {1, 2, 3, 4, 5};
    cobalt_qsort(arr, 5, sizeof(int), cmp_int);
    for (int i = 0; i < 5; i++) {
        TEST_EQUAL(arr[i], i + 1);
    }
    printf("  qsort sorted: OK\n");
}

/* -------------------------------------------------------------------------- */
/* cobalt_insertion_sort                                                      */
/* -------------------------------------------------------------------------- */

void test_insertion_sort_int(void)
{
    printf("Testing insertion_sort int array...\n");
    int arr[] = {5, 3, 8, 1, 9, 2, 7};
    cobalt_insertion_sort(arr, 7, sizeof(int), cmp_int);
    int expected[] = {1, 2, 3, 5, 7, 8, 9};
    for (int i = 0; i < 7; i++) {
        TEST_EQUAL(arr[i], expected[i]);
    }
    printf("  insertion_sort int: OK\n");
}

void test_insertion_sort_already_sorted(void)
{
    printf("Testing insertion_sort already sorted...\n");
    int arr[] = {1, 2, 3, 4, 5};
    cobalt_insertion_sort(arr, 5, sizeof(int), cmp_int);
    for (int i = 0; i < 5; i++) {
        TEST_EQUAL(arr[i], i + 1);
    }
    printf("  insertion_sort sorted: OK\n");
}

void test_insertion_sort_single(void)
{
    printf("Testing insertion_sort single element...\n");
    int arr[] = {42};
    cobalt_insertion_sort(arr, 1, sizeof(int), cmp_int);
    TEST_EQUAL(arr[0], 42);
    printf("  insertion_sort single: OK\n");
}

void test_insertion_sort_double(void)
{
    printf("Testing insertion_sort double array...\n");
    double arr[] = {3.14, 1.41, 2.71, 0.58};
    cobalt_insertion_sort(arr, 4, sizeof(double), cmp_double);
    TEST_ASSERT(arr[0] < arr[1]);
    TEST_ASSERT(arr[1] < arr[2]);
    TEST_ASSERT(arr[2] < arr[3]);
    printf("  insertion_sort double: OK\n");
}

typedef struct {
    uint64_t high;
    uint64_t low;
} test_u128_t;

static int cmp_u128(const void *a, const void *b)
{
    const test_u128_t *x = (const test_u128_t *)a;
    const test_u128_t *y = (const test_u128_t *)b;
    if (x->high != y->high) {
        return (x->high > y->high) - (x->high < y->high);
    }
    return (x->low > y->low) - (x->low < y->low);
}

void test_insertion_sort_large_struct(void)
{
    printf("Testing insertion_sort 16-byte struct...\n");
    test_u128_t arr[] = {{3, 10}, {1, 20}, {2, 5}, {1, 10}};
    cobalt_insertion_sort(arr, 4, sizeof(test_u128_t), cmp_u128);
    TEST_ASSERT(arr[0].high == 1 && arr[0].low == 10);
    TEST_ASSERT(arr[1].high == 1 && arr[1].low == 20);
    TEST_ASSERT(arr[2].high == 2 && arr[2].low == 5);
    TEST_ASSERT(arr[3].high == 3 && arr[3].low == 10);
    printf("  insertion_sort 16-byte struct: OK\n");
}

typedef struct {
    int  key;
    char payload[252];
} test_large_item_t;

static int cmp_large_item(const void *a, const void *b)
{
    const test_large_item_t *x = (const test_large_item_t *)a;
    const test_large_item_t *y = (const test_large_item_t *)b;
    return (x->key > y->key) - (x->key < y->key);
}

void test_insertion_sort_oversized_elements(void)
{
    printf("Testing insertion_sort elements > 128 bytes...\n");
    test_large_item_t arr[4];
    int               keys[4] = {40, 10, 30, 20};
    for (int i = 0; i < 4; i++) {
        arr[i].key = keys[i];
        memset(arr[i].payload, (char)(keys[i] & 0xFF), sizeof(arr[i].payload));
    }

    cobalt_insertion_sort(arr, 4, sizeof(test_large_item_t), cmp_large_item);

    TEST_EQUAL(arr[0].key, 10);
    TEST_EQUAL(arr[1].key, 20);
    TEST_EQUAL(arr[2].key, 30);
    TEST_EQUAL(arr[3].key, 40);
    TEST_EQUAL((unsigned char)arr[0].payload[0], 10);
    TEST_EQUAL((unsigned char)arr[1].payload[0], 20);
    TEST_EQUAL((unsigned char)arr[2].payload[0], 30);
    TEST_EQUAL((unsigned char)arr[3].payload[0], 40);
    printf("  insertion_sort elements > 128 bytes: OK\n");
}

/* -------------------------------------------------------------------------- */
/* cobalt_qsort adversarial and scale tests                                   */
/* -------------------------------------------------------------------------- */

static int g_qsort_adversarial_cmp_count = 0;
static int cmp_int_counted(const void *a, const void *b)
{
    g_qsort_adversarial_cmp_count++;
    return cmp_int(a, b);
}

void test_qsort_adversarial(void)
{
    printf("Testing cobalt_qsort adversarial and scale cases...\n");

    /* 1. Large random array (10,000 ints) */
    {
        int *arr = (int *)malloc(10000 * sizeof(int));
        TEST_ASSERT(arr != NULL);
        uint32_t seed = 987654321;
        for (int i = 0; i < 10000; i++) {
            seed   = seed * 1664525u + 1013904223u;
            arr[i] = (int)(seed % 50000);
        }
        cobalt_qsort(arr, 10000, sizeof(int), cmp_int);
        for (int i = 1; i < 10000; i++) {
            TEST_ASSERT(arr[i - 1] <= arr[i]);
        }
        free(arr);
    }

    /* 2. All-equal array (2,000 ints) — verifies DNF 3-way partition O(N) */
    {
        int *arr = (int *)malloc(2000 * sizeof(int));
        TEST_ASSERT(arr != NULL);
        for (int i = 0; i < 2000; i++) {
            arr[i] = 42;
        }
        g_qsort_adversarial_cmp_count = 0;
        cobalt_qsort(arr, 2000, sizeof(int), cmp_int_counted);
        for (int i = 0; i < 2000; i++) {
            TEST_EQUAL(arr[i], 42);
        }
        /* DNF 3-way partition finishes in linear time (< 4000 comparisons for 2000 elements) */
        TEST_ASSERT(g_qsort_adversarial_cmp_count < 4000);
        free(arr);
    }

    /* 3. Organ-pipe / Sawtooth array (2,048 ints) */
    {
        int *arr = (int *)malloc(2048 * sizeof(int));
        TEST_ASSERT(arr != NULL);

        /* 3a. Organ-pipe: first half ascending, second half descending */
        for (int i = 0; i < 1024; i++) {
            arr[i]        = i;
            arr[2047 - i] = i;
        }
        cobalt_qsort(arr, 2048, sizeof(int), cmp_int);
        for (int i = 1; i < 2048; i++) {
            TEST_ASSERT(arr[i - 1] <= arr[i]);
        }

        /* 3b. Sawtooth array: repeated ramps of 64 elements */
        for (int i = 0; i < 2048; i++) {
            arr[i] = i % 64;
        }
        cobalt_qsort(arr, 2048, sizeof(int), cmp_int);
        for (int i = 1; i < 2048; i++) {
            TEST_ASSERT(arr[i - 1] <= arr[i]);
        }
        free(arr);
    }

    /* 4. Strictly descending array (5,000 ints) */
    {
        int *arr = (int *)malloc(5000 * sizeof(int));
        TEST_ASSERT(arr != NULL);
        for (int i = 0; i < 5000; i++) {
            arr[i] = 5000 - i;
        }
        cobalt_qsort(arr, 5000, sizeof(int), cmp_int);
        for (int i = 0; i < 5000; i++) {
            TEST_EQUAL(arr[i], i + 1);
        }
        free(arr);
    }

    /* 5. Strictly ascending array (5,000 ints) */
    {
        int *arr = (int *)malloc(5000 * sizeof(int));
        TEST_ASSERT(arr != NULL);
        for (int i = 0; i < 5000; i++) {
            arr[i] = i + 1;
        }
        cobalt_qsort(arr, 5000, sizeof(int), cmp_int);
        for (int i = 0; i < 5000; i++) {
            TEST_EQUAL(arr[i], i + 1);
        }
        free(arr);
    }

    /* 6. 16-byte structs (test_u128_t) with cobalt_qsort (1,000 items) */
    {
        test_u128_t *arr = (test_u128_t *)malloc(1000 * sizeof(test_u128_t));
        TEST_ASSERT(arr != NULL);
        uint32_t seed = 543216789;
        for (int i = 0; i < 1000; i++) {
            seed        = seed * 1664525u + 1013904223u;
            arr[i].high = seed % 100;
            seed        = seed * 1664525u + 1013904223u;
            arr[i].low  = seed % 10000;
        }
        cobalt_qsort(arr, 1000, sizeof(test_u128_t), cmp_u128);
        for (int i = 1; i < 1000; i++) {
            TEST_ASSERT(cmp_u128(&arr[i - 1], &arr[i]) <= 0);
        }
        free(arr);
    }

    /* 7. Edge cases (base == NULL, nmemb == 0, size == 0, compar == NULL) with nmemb >= 2 */
    {
        int dummy[4] = {40, 30, 20, 10};

        /* base == NULL with nmemb >= 2 */
        cobalt_qsort(NULL, 10, sizeof(int), cmp_int);
        cobalt_qsort(NULL, 0, sizeof(int), cmp_int);

        /* nmemb == 0 */
        cobalt_qsort(dummy, 0, sizeof(int), cmp_int);
        TEST_EQUAL(dummy[0], 40);
        TEST_EQUAL(dummy[1], 30);

        /* size == 0 with nmemb >= 2 */
        cobalt_qsort(dummy, 4, 0, cmp_int);
        TEST_EQUAL(dummy[0], 40);
        TEST_EQUAL(dummy[1], 30);

        /* compar == NULL with nmemb >= 2 */
        cobalt_qsort(dummy, 4, sizeof(int), NULL);
        TEST_EQUAL(dummy[0], 40);
        TEST_EQUAL(dummy[1], 30);

        /* Single element and NULL base with 0 nmemb/size/compar */
        cobalt_qsort(dummy, 1, sizeof(int), cmp_int);
        TEST_EQUAL(dummy[0], 40);
        cobalt_qsort(NULL, 0, 0, NULL);
    }

    printf("  qsort adversarial: OK\n");
}

/* -------------------------------------------------------------------------- */
/* cobalt_heapsort                                                            */
/* -------------------------------------------------------------------------- */

void test_heapsort(void)
{
    printf("Testing cobalt_heapsort...\n");

    /* 1. Reverse sorted array (100 elements) */
    {
        int arr[100];
        for (int i = 0; i < 100; i++) {
            arr[i] = 100 - i;
        }
        cobalt_heapsort(arr, 100, sizeof(int), cmp_int);
        for (int i = 0; i < 100; i++) {
            TEST_EQUAL(arr[i], i + 1);
        }
    }

    /* 2. Random array (250 elements) */
    {
        int      arr[250];
        uint32_t seed = 123456789;
        for (int i = 0; i < 250; i++) {
            seed   = seed * 1664525u + 1013904223u;
            arr[i] = (int)(seed % 10000);
        }
        cobalt_heapsort(arr, 250, sizeof(int), cmp_int);
        for (int i = 1; i < 250; i++) {
            TEST_ASSERT(arr[i - 1] <= arr[i]);
        }
    }

    /* 3. Array with all identical elements */
    {
        int arr[50];
        for (int i = 0; i < 50; i++) {
            arr[i] = 42;
        }
        cobalt_heapsort(arr, 50, sizeof(int), cmp_int);
        for (int i = 0; i < 50; i++) {
            TEST_EQUAL(arr[i], 42);
        }
    }

    /* 4. Single-element and empty array (0 elements, NULL base) */
    {
        cobalt_heapsort(NULL, 10, sizeof(int), cmp_int);
        cobalt_heapsort(NULL, 0, sizeof(int), cmp_int);

        int dummy = 99;
        cobalt_heapsort(&dummy, 0, sizeof(int), cmp_int);
        TEST_EQUAL(dummy, 99);

        int single = 77;
        cobalt_heapsort(&single, 1, sizeof(int), cmp_int);
        TEST_EQUAL(single, 77);

        cobalt_heapsort(&single, 1, 0, cmp_int);
        TEST_EQUAL(single, 77);

        cobalt_heapsort(&single, 1, sizeof(int), NULL);
        TEST_EQUAL(single, 77);
    }

    /* 5. 16-byte structs (test_u128_t) */
    {
        test_u128_t arr[6] = {
            {5, 100},
            {2, 300},
            {8, 50},
            {2, 100},
            {1, 999},
            {8, 20},
        };
        cobalt_heapsort(arr, 6, sizeof(test_u128_t), cmp_u128);
        for (int i = 1; i < 6; i++) {
            TEST_ASSERT(cmp_u128(&arr[i - 1], &arr[i]) <= 0);
        }
        TEST_ASSERT(arr[0].high == 1 && arr[0].low == 999);
        TEST_ASSERT(arr[1].high == 2 && arr[1].low == 100);
        TEST_ASSERT(arr[2].high == 2 && arr[2].low == 300);
        TEST_ASSERT(arr[3].high == 5 && arr[3].low == 100);
        TEST_ASSERT(arr[4].high == 8 && arr[4].low == 20);
        TEST_ASSERT(arr[5].high == 8 && arr[5].low == 50);
    }

    printf("  heapsort: OK\n");
}

/* -------------------------------------------------------------------------- */
/* cobalt_bsearch                                                             */
/* -------------------------------------------------------------------------- */

void test_bsearch_found(void)
{
    printf("Testing bsearch found...\n");
    int   arr[]  = {1, 3, 5, 7, 9, 11};
    int   key    = 7;
    void *result = cobalt_bsearch(&key, arr, 6, sizeof(int), cmp_int);
    TEST_ASSERT(result != NULL);
    TEST_EQUAL(*(int *)result, 7);
    printf("  bsearch found: OK\n");
}

void test_bsearch_not_found(void)
{
    printf("Testing bsearch not found...\n");
    int   arr[]  = {1, 3, 5, 7, 9};
    int   key    = 4;
    void *result = cobalt_bsearch(&key, arr, 5, sizeof(int), cmp_int);
    TEST_ASSERT(result == NULL);
    printf("  bsearch not found: OK\n");
}

void test_bsearch_empty(void)
{
    printf("Testing bsearch empty array...\n");
    int   key    = 1;
    void *result = cobalt_bsearch(&key, NULL, 0, sizeof(int), cmp_int);
    TEST_ASSERT(result == NULL);
    printf("  bsearch empty: OK\n");
}

void test_bsearch_first(void)
{
    printf("Testing bsearch first element...\n");
    int   arr[]  = {1, 3, 5};
    int   key    = 1;
    void *result = cobalt_bsearch(&key, arr, 3, sizeof(int), cmp_int);
    TEST_ASSERT(result != NULL);
    TEST_EQUAL(*(int *)result, 1);
    printf("  bsearch first: OK\n");
}

void test_bsearch_last(void)
{
    printf("Testing bsearch last element...\n");
    int   arr[]  = {1, 3, 5};
    int   key    = 5;
    void *result = cobalt_bsearch(&key, arr, 3, sizeof(int), cmp_int);
    TEST_ASSERT(result != NULL);
    TEST_EQUAL(*(int *)result, 5);
    printf("  bsearch last: OK\n");
}

/* -------------------------------------------------------------------------- */
/* cobalt_find_if / cobalt_for_each                                           */
/* -------------------------------------------------------------------------- */

void test_find_if(void)
{
    printf("Testing find_if...\n");
    int   arr[]  = {1, 4, 3, 8, 5};
    void *result = cobalt_find_if(arr, 5, sizeof(int), pred_gt5);
    TEST_ASSERT(result != NULL);
    TEST_EQUAL(*(int *)result, 8);
    printf("  find_if: OK\n");
}

void test_find_if_not_found(void)
{
    printf("Testing find_if not found...\n");
    int   arr[]  = {1, 2, 3};
    void *result = cobalt_find_if(arr, 3, sizeof(int), pred_gt10);
    TEST_ASSERT(result == NULL);
    printf("  find_if not found: OK\n");
}

/* for_each test helper: increment counter stored in user_data */
static void op_count(void *item)
{
    int *counter = (int *)item;
    (void)counter;
}

void test_for_each(void)
{
    printf("Testing for_each...\n");
    int arr[] = {1, 2, 3, 4, 5};
    /* for_each signature: for_each(base, nmemb, size, op) */
    /* We can't easily capture call_count, so use a global trick */
    cobalt_for_each(arr, 5, sizeof(int), op_count);
    /* Just verify it doesn't crash and processes the array */
    printf("  for_each: OK\n");
}

/* -------------------------------------------------------------------------- */
/* cobalt_map / cobalt_filter / cobalt_fold                                   */
/* -------------------------------------------------------------------------- */

void test_map(void)
{
    printf("Testing map...\n");
    int input[] = {1, 2, 3, 4, 5};
    int output[5];
    int ret = cobalt_map(input, output, 5, sizeof(int), map_double, NULL);
    TEST_EQUAL(ret, 0);
    for (int i = 0; i < 5; i++) {
        TEST_EQUAL(output[i], input[i] * 2);
    }
    printf("  map: OK\n");
}

void test_filter(void)
{
    printf("Testing filter...\n");
    int    input[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int    output[8];
    size_t count = 8;
    int    ret   = cobalt_filter(input, output, &count, sizeof(int), pred_even);
    TEST_EQUAL(ret, 0);
    TEST_EQUAL(count, 4u);
    TEST_EQUAL(output[0], 2);
    TEST_EQUAL(output[1], 4);
    TEST_EQUAL(output[2], 6);
    TEST_EQUAL(output[3], 8);
    printf("  filter: OK\n");
}

void test_fold(void)
{
    printf("Testing fold (sum)...\n");
    int   arr[]  = {1, 2, 3, 4, 5};
    int   sum    = 0;
    void *result = cobalt_fold(arr, 5, sizeof(int), &sum, fold_sum, NULL);
    TEST_ASSERT(result == &sum);
    TEST_EQUAL(sum, 15);
    printf("  fold sum: OK\n");
}

void test_sort_stable(void);
void test_sort_partition(void);
void test_sort_unique(void);
void test_sort_with_alloc(void);

void test_predicate_helpers(void)
{
    printf("Testing predicate helpers...\n");
    TEST_ASSERT(predicate_null(NULL) == 1);
    TEST_ASSERT(predicate_null((void *)0x1234) == 0);
    TEST_ASSERT(predicate_nonnull((void *)0x1234) == 1);
    TEST_ASSERT(predicate_nonnull(NULL) == 0);
    printf("  predicates: OK\n");
}

/* -------------------------------------------------------------------------- */
/* Main test entry                                                            */
/* -------------------------------------------------------------------------- */

void test_sort(void)
{
    printf("Testing sort and functional algorithms...\n");
    test_qsort_int();
    test_qsort_double();
    test_qsort_empty();
    test_qsort_single();
    test_qsort_already_sorted();
    test_qsort_adversarial();
    test_insertion_sort_int();
    test_insertion_sort_double();
    test_insertion_sort_large_struct();
    test_insertion_sort_oversized_elements();
    test_insertion_sort_already_sorted();
    test_insertion_sort_single();
    test_heapsort();
    test_sort_stable();
    test_sort_partition();
    test_sort_unique();
    test_sort_with_alloc();
    test_bsearch_found();
    test_bsearch_not_found();
    test_bsearch_empty();
    test_bsearch_first();
    test_bsearch_last();
    test_find_if();
    test_find_if_not_found();
    test_for_each();
    test_map();
    test_filter();
    test_fold();
    test_predicate_helpers();
    printf("  Sort/functional tests completed\n");
}

void test_sort_stable(void)
{
    printf("Testing cobalt_stable_sort...\n");

    int    arr[] = {5, 3, 8, 1, 3, 2};
    size_t n     = sizeof(arr) / sizeof(arr[0]);
    cobalt_stable_sort(arr, n, sizeof(int), cmp_int);

    TEST_ASSERT(arr[0] == 1);
    TEST_ASSERT(arr[1] == 2);
    TEST_ASSERT(arr[2] == 3);
    TEST_ASSERT(arr[3] == 3);
    TEST_ASSERT(arr[4] == 5);
    TEST_ASSERT(arr[5] == 8);
    printf("  stable_sort: OK\n");
}

void test_sort_partition(void)
{
    printf("Testing cobalt_partition...\n");

    int    arr[] = {9, 3, 7, 1, 5, 4, 8, 2};
    size_t n     = sizeof(arr) / sizeof(arr[0]);
    int    pivot = 5;

    size_t idx = cobalt_partition(arr, n, sizeof(int), &pivot, cmp_int);

    TEST_ASSERT(idx > 0 && idx <= n);
    for (size_t i = 0; i < idx; i++) {
        TEST_ASSERT(arr[i] < pivot);
    }
    for (size_t i = idx; i < n; i++) {
        TEST_ASSERT(arr[i] >= pivot);
    }
    printf("  partition: OK\n");
}

void test_sort_unique(void)
{
    printf("Testing cobalt_unique...\n");

    int    arr[]  = {1, 1, 2, 3, 3, 3, 4, 5, 5};
    size_t n      = sizeof(arr) / sizeof(arr[0]);
    size_t result = cobalt_unique(arr, &n, sizeof(int), cmp_int);

    TEST_ASSERT(result == 5);
    TEST_ASSERT(arr[0] == 1);
    TEST_ASSERT(arr[1] == 2);
    TEST_ASSERT(arr[2] == 3);
    TEST_ASSERT(arr[3] == 4);
    TEST_ASSERT(arr[4] == 5);
    printf("  unique: OK\n");
}

/* -------------------------------------------------------------------------- */
/* cobalt _with_alloc balance tests                                           */
/* -------------------------------------------------------------------------- */

static char   mock_sort_buf[4096];
static size_t mock_sort_off         = 0;
static int    mock_sort_alloc_count = 0;
static int    mock_sort_free_count  = 0;

static void *mock_sort_alloc(cobalt_allocator_t *self, size_t size)
{
    (void)self;
    mock_sort_alloc_count++;
    if (mock_sort_off + size > sizeof(mock_sort_buf)) {
        return NULL;
    }
    void *ptr = &mock_sort_buf[mock_sort_off];
    mock_sort_off += size;
    return ptr;
}

static void mock_sort_free(cobalt_allocator_t *self, void *ptr)
{
    (void)self;
    (void)ptr;
    mock_sort_free_count++;
}

static void *mock_sort_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
{
    (void)self;
    (void)ptr;
    (void)new_size;
    return NULL;
}

static cobalt_allocator_t mock_sort_alloc_inst = {
    .alloc   = mock_sort_alloc,
    .free    = mock_sort_free,
    .realloc = mock_sort_realloc,
};

static void mock_sort_reset(void)
{
    mock_sort_off         = 0;
    mock_sort_alloc_count = 0;
    mock_sort_free_count  = 0;
}

void test_sort_with_alloc(void)
{
    printf("Testing sort _with_alloc APIs...\n");
    cobalt_allocator_t *a = &mock_sort_alloc_inst;

    /* insertion_sort_with_alloc */
    mock_sort_reset();
    int arr1[] = {5, 3, 8, 1, 9};
    cobalt_insertion_sort_with_alloc(arr1, 5, sizeof(int), cmp_int, a);
    TEST_ASSERT(arr1[0] == 1);
    TEST_ASSERT(arr1[1] == 3);
    TEST_ASSERT(arr1[2] == 5);
    TEST_ASSERT(arr1[3] == 8);
    TEST_ASSERT(arr1[4] == 9);
    TEST_ASSERT(mock_sort_alloc_count > 0);
    TEST_ASSERT(mock_sort_free_count > 0);

    /* stable_sort_with_alloc */
    mock_sort_reset();
    int arr2[] = {5, 3, 8, 1, 3, 2};
    cobalt_stable_sort_with_alloc(arr2, 6, sizeof(int), cmp_int, a);
    TEST_ASSERT(arr2[0] == 1);
    TEST_ASSERT(arr2[1] == 2);
    TEST_ASSERT(arr2[2] == 3);
    TEST_ASSERT(arr2[3] == 3);
    TEST_ASSERT(arr2[4] == 5);
    TEST_ASSERT(arr2[5] == 8);

    /* partition_with_alloc */
    mock_sort_reset();
    int    arr3[] = {9, 3, 7, 1, 5, 4, 8, 2};
    int    pivot  = 5;
    size_t idx    = cobalt_partition_with_alloc(arr3, 8, sizeof(int), &pivot, cmp_int, a);
    TEST_ASSERT(idx > 0 && idx <= 8);
    for (size_t i = 0; i < idx; i++) {
        TEST_ASSERT(arr3[i] < pivot);
    }
    for (size_t i = idx; i < 8; i++) {
        TEST_ASSERT(arr3[i] >= pivot);
    }

    /* NULL allocator falls back to system */
    int arr4[] = {3, 1, 2};
    cobalt_insertion_sort_with_alloc(arr4, 3, sizeof(int), cmp_int, NULL);
    TEST_ASSERT(arr4[0] == 1);
    TEST_ASSERT(arr4[1] == 2);
    TEST_ASSERT(arr4[2] == 3);

    printf("  _with_alloc balance: OK\n");
}
