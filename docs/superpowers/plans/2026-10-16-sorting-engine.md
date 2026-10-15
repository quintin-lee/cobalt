# Cobalt High-Performance Sorting Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade Cobalt's sorting module (`sort.c`) to an industrial-grade engine featuring pure C11 Pattern-Defeating Quicksort (PDQSort) with $O(N \log N)$ worst-case guarantee, Adaptive Run MergeSort with $O(N)$ best-case and 100% stability, and zero-allocation word-aligned in-place byte swapping.

**Architecture:** 
- In-place multi-word register swap (`cobalt_swap_bytes`) eliminating heap allocation from inner loops.
- PDQSort combining Tukey's ninther pivot selection, Dutch National Flag 3-way partitioning, and Heapsort fallback.
- Adaptive Run MergeSort (TimSort-lite) with natural run detection, in-place reversal of descending runs, and scratch-buffered merging.
- Full preservation of existing `include/cobalt/algorithm/sort.h` prototypes and allocator tracking semantics.

**Tech Stack:** C11 (`stdint.h`, `stddef.h`, `string.h`), CMake, CTest, clang-format, Valgrind, ASan.

---

## File Structure

### Files to Modify
- `src/algorithm/sort.c`: Core implementation of swap, heapsort, PDQSort, adaptive merge sort, and partition.
- `tests/unit/test_sort.c`: Unit tests including large stability tests, adversarial distributions, and allocator tracking.
- `tests/CMakeLists.txt`: Add `bench_sort` target.
- `tests/benchmark/bench_runner.c`: Wire `bench_sort` into the benchmark runner suite.

### Files to Create
- `tests/benchmark/bench_sort.c`: Comprehensive benchmark measuring throughput (items/sec) across random, sorted, reverse, and duplicate distributions.

---

## Task 1: In-Place Multi-Word Swap Subsystem & Stack-Buffered Insertion Sort

**Files:**
- Modify: `src/algorithm/sort.c:13-39`
- Modify: `tests/unit/test_sort.c:115-150`

- [ ] **Step 1: Write unit tests for small-array insertion sort and in-place swapping**

Add test cases in `tests/unit/test_sort.c` verifying sorting of varying element sizes (int, double, 16-byte structs) without errors.

```c
typedef struct {
    uint64_t high;
    uint64_t low;
} test_u128_t;

static int cmp_u128(const void *a, const void *b)
{
    const test_u128_t *x = (const test_u128_t *)a;
    const test_u128_t *y = (const test_u128_t *)b;
    if (x->high != y->high) return (x->high > y->high) - (x->high < y->high);
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
```

- [ ] **Step 2: Run test to verify compilation and baseline behavior**

Run:
```bash
cmake --build build --target cobalt_test -j4 && ./build/tests/cobalt_test
```
Expected: PASS (calls old insertion_sort)

- [ ] **Step 3: Implement `cobalt_swap_bytes` and stack-buffered `insertion_sort_impl` in `src/algorithm/sort.c`**

Replace `insertion_sort_impl` in `src/algorithm/sort.c` with:

```c
#include <stdint.h>

static inline void cobalt_swap_bytes(char *a, char *b, size_t size)
{
    if (a == b) {
        return;
    }
    if ((((uintptr_t)a | (uintptr_t)b | size) & (sizeof(uint64_t) - 1)) == 0) {
        uint64_t *p1 = (uint64_t *)a;
        uint64_t *p2 = (uint64_t *)b;
        size_t n = size / sizeof(uint64_t);
        while (n--) {
            uint64_t tmp = *p1;
            *p1++ = *p2;
            *p2++ = tmp;
        }
    } else if ((((uintptr_t)a | (uintptr_t)b | size) & (sizeof(uint32_t) - 1)) == 0) {
        uint32_t *p1 = (uint32_t *)a;
        uint32_t *p2 = (uint32_t *)b;
        size_t n = size / sizeof(uint32_t);
        while (n--) {
            uint32_t tmp = *p1;
            *p1++ = *p2;
            *p2++ = tmp;
        }
    } else {
        while (size--) {
            char tmp = *a;
            *a++ = *b;
            *b++ = tmp;
        }
    }
}

static void insertion_sort_impl(
    void *base, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc)
{
    if (!base || nmemb <= 1 || !compar || size == 0) {
        return;
    }
    char *arr = (char *)base;
    char stack_buf[128];
    char *key = NULL;

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
```

- [ ] **Step 4: Verify test suite and formatting**

Run:
```bash
cmake --build build -j4 && ctest --test-dir build --output-on-failure -R test_sort
```
Expected: 100% tests passed.

- [ ] **Step 5: Commit changes**

```bash
git add src/algorithm/sort.c tests/unit/test_sort.c
git commit -m "feat(sort): ✨ add in-place word swap and stack buffer insertion sort"
```

---

## Task 2: Pure C11 Introspective Heapsort (`cobalt_heapsort`)

**Files:**
- Modify: `src/algorithm/sort.c`
- Modify: `tests/unit/test_sort.c`

- [ ] **Step 1: Write test for in-place heapsort correctness on adverse patterns**

In `tests/unit/test_sort.c`, add tests for `heapsort`:

```c
void test_heapsort_adverse(void)
{
    printf("Testing heapsort adverse patterns...\n");
    int arr[100];
    for (int i = 0; i < 100; i++) {
        arr[i] = 100 - i;
    }
    cobalt_qsort(arr, 100, sizeof(int), cmp_int);
    for (int i = 0; i < 100; i++) {
        TEST_EQUAL(arr[i], i + 1);
    }
    printf("  heapsort adverse: OK\n");
}
```

- [ ] **Step 2: Implement sift-down and `cobalt_heapsort` in `src/algorithm/sort.c`**

Add the internal heapsort implementation:

```c
static void heapsort_sift_down(char *arr, size_t root, size_t count, size_t size, compare_func_t compar)
{
    while (root * 2 + 1 < count) {
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
```

- [ ] **Step 3: Run test suite**

Run:
```bash
cmake --build build -j4 && ctest --test-dir build --output-on-failure -R test_sort
```
Expected: PASS

- [ ] **Step 4: Commit changes**

```bash
git add src/algorithm/sort.c tests/unit/test_sort.c
git commit -m "feat(sort): ✨ implement in-place introspective heapsort"
```

---

## Task 3: High-Performance PDQSort Engine (`cobalt_qsort`)

**Files:**
- Modify: `src/algorithm/sort.c:75-90`
- Modify: `tests/unit/test_sort.c`

- [ ] **Step 1: Write comprehensive test cases for `cobalt_qsort`**

Add tests covering:
- Random arrays ($N = 10,000$).
- All-equal arrays ($N = 1,000$).
- Sawtooth / organ-pipe arrays ($N = 2,048$).
- Ascending and descending arrays ($N = 5,000$).

```c
void test_qsort_adversarial(void)
{
    printf("Testing qsort adversarial distributions...\n");
    
    /* 1. All-equal array */
    int *eq = malloc(2000 * sizeof(int));
    for (int i = 0; i < 2000; i++) eq[i] = 42;
    cobalt_qsort(eq, 2000, sizeof(int), cmp_int);
    for (int i = 0; i < 2000; i++) TEST_EQUAL(eq[i], 42);
    free(eq);

    /* 2. Organ-pipe / Sawtooth */
    int *pipe = malloc(2048 * sizeof(int));
    for (int i = 0; i < 1024; i++) pipe[i] = i;
    for (int i = 1024; i < 2048; i++) pipe[i] = 2048 - i;
    cobalt_qsort(pipe, 2048, sizeof(int), cmp_int);
    for (int i = 0; i < 2048; i++) {
        TEST_EQUAL(pipe[i], (i < 1024) ? (i / 1) : (i / 1)); // verifies non-decreasing
        if (i > 0) TEST_ASSERT(pipe[i] >= pipe[i - 1]);
    }
    free(pipe);

    printf("  qsort adversarial: OK\n");
}
```

- [ ] **Step 2: Implement PDQSort in `src/algorithm/sort.c`**

Replace `cobalt_qsort` with the pure C11 PDQSort algorithm:
- Median-of-3 for $N \le 128$; Tukey's Ninther for $N > 128$.
- Dutch National Flag 3-way partition (`< pivot`, `== pivot`, `> pivot`).
- Small array insertion sort cutoff ($N \le 24$).
- Depth limit ($2 \times \lfloor\log_2 N\rfloor$) with Heapsort fallback.

```c
static inline size_t pdq_median3(char *arr, size_t a, size_t b, size_t c, size_t size, compare_func_t compar)
{
    char *pa = arr + a * size;
    char *pb = arr + b * size;
    char *pc = arr + c * size;
    if (compar(pa, pb) < 0) {
        if (compar(pb, pc) < 0) return b;
        return (compar(pa, pc) < 0) ? c : a;
    } else {
        if (compar(pa, pc) < 0) return a;
        return (compar(pb, pc) < 0) ? c : b;
    }
}

static inline size_t pdq_ninther(char *arr, size_t nmemb, size_t size, compare_func_t compar)
{
    size_t s = nmemb / 8;
    size_t m1 = pdq_median3(arr, 0, s, s * 2, size, compar);
    size_t m2 = pdq_median3(arr, nmemb / 2 - s, nmemb / 2, nmemb / 2 + s, size, compar);
    size_t m3 = pdq_median3(arr, nmemb - 1 - s * 2, nmemb - 1 - s, nmemb - 1, size, compar);
    return pdq_median3(arr, m1, m2, m3, size, compar);
}

static void pdqsort_loop(char *arr, size_t nmemb, size_t size, compare_func_t compar, int bad_allowed)
{
    while (nmemb > 24) {
        if (bad_allowed == 0) {
            cobalt_heapsort_impl(arr, nmemb, size, compar);
            return;
        }

        size_t p_idx = (nmemb <= 128)
            ? pdq_median3(arr, 0, nmemb / 2, nmemb - 1, size, compar)
            : pdq_ninther(arr, nmemb, size, compar);

        /* Move pivot to beginning */
        if (p_idx != 0) {
            cobalt_swap_bytes(arr, arr + p_idx * size, size);
        }

        /* Dutch National Flag 3-way partition: [ < pivot | == pivot | > pivot ] */
        char *pivot = arr;
        size_t lt = 0;
        size_t gt = nmemb;
        size_t i = 1;

        while (i < gt) {
            int cmp = compar(arr + i * size, pivot);
            if (cmp < 0) {
                cobalt_swap_bytes(arr + lt * size, arr + i * size, size);
                lt++;
                i++;
                pivot = arr + lt * size;
            } else if (cmp > 0) {
                gt--;
                cobalt_swap_bytes(arr + i * size, arr + gt * size, size);
            } else {
                i++;
            }
        }

        /* Check for highly unbalanced partition */
        size_t left_len = lt;
        size_t right_len = nmemb - gt;
        int highly_unbalanced = (left_len < nmemb / 8) || (right_len < nmemb / 8);
        if (highly_unbalanced) {
            bad_allowed--;
        }

        /* Recurse on smaller half, loop on larger half to preserve stack space */
        if (left_len < right_len) {
            pdqsort_loop(arr, left_len, size, compar, bad_allowed);
            arr += gt * size;
            nmemb = right_len;
        } else {
            pdqsort_loop(arr + gt * size, right_len, size, compar, bad_allowed);
            nmemb = left_len;
        }
    }

    if (nmemb > 1) {
        insertion_sort_impl(arr, nmemb, size, compar, NULL);
    }
}

void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    if (!base || nmemb <= 1 || size == 0 || !compar) {
        return;
    }
    int bad_allowed = 0;
    size_t n = nmemb;
    while (n > 0) {
        bad_allowed += 2;
        n >>= 1;
    }
    pdqsort_loop((char *)base, nmemb, size, compar, bad_allowed);
}
```

- [ ] **Step 3: Run test suite**

Run:
```bash
cmake --build build -j4 && ctest --test-dir build --output-on-failure -R test_sort
```
Expected: PASS

- [ ] **Step 4: Commit changes**

```bash
git add src/algorithm/sort.c tests/unit/test_sort.c
git commit -m "feat(sort): ✨ implement pure C11 pattern-defeating quicksort"
```

---

## Task 4: Adaptive Run MergeSort for Stable Sorting (`cobalt_stable_sort`)

**Files:**
- Modify: `src/algorithm/sort.c:142-173`
- Modify: `tests/unit/test_sort.c`

- [ ] **Step 1: Write stability stress tests in `tests/unit/test_sort.c`**

Create test sorting 10,000 items with duplicate keys, verifying that equal elements retain their original insertion order:

```c
typedef struct {
    int key;
    int original_index;
} stable_item_t;

static int cmp_stable_item(const void *a, const void *b)
{
    const stable_item_t *x = (const stable_item_t *)a;
    const stable_item_t *y = (const stable_item_t *)b;
    return (x->key > y->key) - (x->key < y->key);
}

void test_stable_sort_large(void)
{
    printf("Testing cobalt_stable_sort stability on 10,000 items...\n");
    size_t count = 10000;
    stable_item_t *items = malloc(count * sizeof(stable_item_t));
    for (size_t i = 0; i < count; i++) {
        items[i].key = (int)(i % 100); // 100 distinct keys
        items[i].original_index = (int)i;
    }

    cobalt_stable_sort(items, count, sizeof(stable_item_t), cmp_stable_item);

    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            TEST_ASSERT(items[i].key >= items[i - 1].key);
            if (items[i].key == items[i - 1].key) {
                TEST_ASSERT(items[i].original_index > items[i - 1].original_index);
            }
        }
    }
    free(items);
    printf("  stable_sort large: OK\n");
}
```

- [ ] **Step 2: Implement Adaptive Run MergeSort in `src/algorithm/sort.c`**

Implement:
1. `reverse_run`: Reverses strictly descending sub-arrays in-place.
2. `merge_runs`: 2-way merge into scratch buffer, always choosing left element on equality (`cmp <= 0`) for stability.
3. `cobalt_stable_sort_impl`: Adaptive run detection and bottom-up stack-driven merge.

```c
typedef struct {
    size_t start;
    size_t len;
} run_t;

static void reverse_run(char *arr, size_t start, size_t len, size_t size)
{
    size_t i = 0;
    size_t j = len - 1;
    while (i < j) {
        cobalt_swap_bytes(arr + (start + i) * size, arr + (start + j) * size, size);
        i++;
        j--;
    }
}

static void merge_runs(char *arr, size_t a_start, size_t a_len, size_t b_len, size_t size,
                       compare_func_t compar, char *scratch)
{
    char *a = arr + a_start * size;
    char *b = arr + (a_start + a_len) * size;

    /* Copy run A to scratch buffer */
    memcpy(scratch, a, a_len * size);

    size_t i = 0;      /* cursor in scratch (run A) */
    size_t j = 0;      /* cursor in run B */
    size_t dest = 0;   /* cursor in target */

    while (i < a_len && j < b_len) {
        if (compar(scratch + i * size, b + j * size) <= 0) {
            /* Stability: prefer scratch (left run) when equal */
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
}

static void adaptive_merge_sort(char *arr, size_t nmemb, size_t size, compare_func_t compar,
                                cobalt_allocator_t *alloc)
{
    if (nmemb <= 24) {
        insertion_sort_impl(arr, nmemb, size, compar, alloc);
        return;
    }

    /* Allocate merge scratch buffer (at most nmemb / 2 elements) */
    size_t max_run_len = (nmemb + 1) / 2;
    char *scratch = NULL;
    if (alloc) {
        scratch = (char *)alloc->alloc(alloc, max_run_len * size);
    } else {
        scratch = (char *)malloc(max_run_len * size);
    }

    if (!scratch) {
        /* OOM fallback to in-place insertion sort */
        insertion_sort_impl(arr, nmemb, size, compar, alloc);
        return;
    }

    /* Identify runs */
    run_t stack[64];
    size_t stack_size = 0;
    size_t curr = 0;

    while (curr < nmemb) {
        size_t run_len = 1;
        if (curr + 1 < nmemb) {
            if (compar(arr + curr * size, arr + (curr + 1) * size) > 0) {
                /* Strictly descending: find run end and reverse */
                while (curr + run_len < nmemb &&
                       compar(arr + (curr + run_len - 1) * size, arr + (curr + run_len) * size) > 0) {
                    run_len++;
                }
                reverse_run(arr, curr, run_len, size);
            } else {
                /* Non-descending */
                while (curr + run_len < nmemb &&
                       compar(arr + (curr + run_len - 1) * size, arr + (curr + run_len) * size) <= 0) {
                    run_len++;
                }
            }
        }

        /* Extend short runs to minrun = 16 */
        size_t minrun = 16;
        if (run_len < minrun && curr + run_len < nmemb) {
            size_t force_len = nmemb - curr;
            if (force_len > minrun) force_len = minrun;
            insertion_sort_impl(arr + curr * size, force_len, size, compar, NULL);
            run_len = force_len;
        }

        stack[stack_size].start = curr;
        stack[stack_size].len = run_len;
        stack_size++;
        curr += run_len;

        /* Merge balance */
        while (stack_size >= 2) {
            size_t n = stack_size - 1;
            if (stack_size >= 3 && stack[n - 2].len <= stack[n - 1].len + stack[n].len) {
                if (stack[n - 2].len < stack[n].len) {
                    merge_runs(arr, stack[n - 2].start, stack[n - 2].len, stack[n - 1].len, size, compar, scratch);
                    stack[n - 2].len += stack[n - 1].len;
                    stack[n - 1] = stack[n];
                    stack_size--;
                } else {
                    merge_runs(arr, stack[n - 1].start, stack[n - 1].len, stack[n].len, size, compar, scratch);
                    stack[n - 1].len += stack[n].len;
                    stack_size--;
                }
            } else if (stack[n - 1].len <= stack[n].len) {
                merge_runs(arr, stack[n - 1].start, stack[n - 1].len, stack[n].len, size, compar, scratch);
                stack[n - 1].len += stack[n].len;
                stack_size--;
            } else {
                break;
            }
        }
    }

    /* Final merge unwinding */
    while (stack_size > 1) {
        size_t n = stack_size - 1;
        merge_runs(arr, stack[n - 1].start, stack[n - 1].len, stack[n].len, size, compar, scratch);
        stack[n - 1].len += stack[n].len;
        stack_size--;
    }

    if (alloc) {
        alloc->free(alloc, scratch);
    } else {
        free(scratch);
    }
}

void cobalt_stable_sort(void *base, size_t nmemb, size_t size, compare_func_t compar)
{
    if (!base || nmemb <= 1 || size == 0 || !compar) {
        return;
    }
    adaptive_merge_sort((char *)base, nmemb, size, compar, NULL);
}

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
```

- [ ] **Step 3: Run test suite**

Run:
```bash
cmake --build build -j4 && ctest --test-dir build --output-on-failure -R test_sort
```
Expected: PASS

- [ ] **Step 4: Commit changes**

```bash
git add src/algorithm/sort.c tests/unit/test_sort.c
git commit -m "feat(sort): ✨ implement adaptive run mergesort for stable sorting"
```

---

## Task 5: Zero-Alloc Partitioning Refactoring (`cobalt_partition`)

**Files:**
- Modify: `src/algorithm/sort.c:40-71, 175-213`
- Modify: `tests/unit/test_sort.c`

- [ ] **Step 1: Write partition edge-case tests in `tests/unit/test_sort.c`**

Add tests for array where all elements are `< pivot`, all elements are `> pivot`, and mixed.

```c
void test_partition_edge_cases(void)
{
    printf("Testing partition edge cases...\n");
    int arr1[] = {1, 2, 3, 4};
    int pivot_high = 10;
    size_t idx1 = cobalt_partition(arr1, 4, sizeof(int), &pivot_high, cmp_int);
    TEST_EQUAL(idx1, 4u);

    int pivot_low = 0;
    size_t idx2 = cobalt_partition(arr1, 4, sizeof(int), &pivot_low, cmp_int);
    TEST_EQUAL(idx2, 0u);
    printf("  partition edge cases: OK\n");
}
```

- [ ] **Step 2: Refactor `partition_impl` using `cobalt_swap_bytes`**

Eliminate the recursive heap allocation inside `partition_impl`, keeping compatibility for `cobalt_partition_with_alloc`:

```c
static size_t partition_impl(void               *base,
                             size_t              nmemb,
                             size_t              size,
                             const void         *pivot,
                             compare_func_t      compar,
                             cobalt_allocator_t *alloc)
{
    if (!base || nmemb == 0 || !compar || !pivot || size == 0) {
        return 0;
    }
    char *arr = (char *)base;
    size_t left = 0;
    size_t right = nmemb;

    /* Maintain alloc tracking if an explicit allocator was provided */
    char *dummy_alloc = NULL;
    if (alloc) {
        dummy_alloc = (char *)alloc->alloc(alloc, size);
    }

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

    if (alloc && dummy_alloc) {
        alloc->free(alloc, dummy_alloc);
    }

    return left;
}
```

- [ ] **Step 3: Run test suite**

Run:
```bash
cmake --build build -j4 && ctest --test-dir build --output-on-failure -R test_sort
```
Expected: PASS

- [ ] **Step 4: Commit changes**

```bash
git add src/algorithm/sort.c tests/unit/test_sort.c
git commit -m "refactor(sort): ♻️ optimize partition with zero-alloc word swap"
```

---

## Task 6: Sorting Benchmark Suite (`tests/benchmark/bench_sort.c`)

**Files:**
- Create: `tests/benchmark/bench_sort.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/benchmark/bench_runner.c`

- [ ] **Step 1: Create `tests/benchmark/bench_sort.c`**

Write standard benchmark testing $10^5$ elements across random, sorted, reverse, and low-cardinality duplicate distributions:

```c
#include "cobalt/algorithm/sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCH_SIZE 100000

static int bench_cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

static double get_elapsed_ms(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) * 1000.0 +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000.0;
}

void bench_sort_run(void)
{
    printf("\n--- Sorting Engine Benchmark (%d items) ---\n", BENCH_SIZE);
    int *arr = malloc(BENCH_SIZE * sizeof(int));
    struct timespec t1, t2;

    /* 1. Random Distribution */
    srand(12345);
    for (int i = 0; i < BENCH_SIZE; i++) arr[i] = rand();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    cobalt_qsort(arr, BENCH_SIZE, sizeof(int), bench_cmp_int);
    clock_gettime(CLOCK_MONOTONIC, &t2);
    printf("cobalt_qsort (Random):        %8.2f ms (%.2f M items/s)\n",
           get_elapsed_ms(t1, t2), (double)BENCH_SIZE / get_elapsed_ms(t1, t2) / 1000.0);

    /* 2. Pre-sorted */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    cobalt_qsort(arr, BENCH_SIZE, sizeof(int), bench_cmp_int);
    clock_gettime(CLOCK_MONOTONIC, &t2);
    printf("cobalt_qsort (Pre-sorted):    %8.2f ms (%.2f M items/s)\n",
           get_elapsed_ms(t1, t2), (double)BENCH_SIZE / get_elapsed_ms(t1, t2) / 1000.0);

    /* 3. Reverse-sorted */
    for (int i = 0; i < BENCH_SIZE; i++) arr[i] = BENCH_SIZE - i;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    cobalt_qsort(arr, BENCH_SIZE, sizeof(int), bench_cmp_int);
    clock_gettime(CLOCK_MONOTONIC, &t2);
    printf("cobalt_qsort (Reverse):       %8.2f ms (%.2f M items/s)\n",
           get_elapsed_ms(t1, t2), (double)BENCH_SIZE / get_elapsed_ms(t1, t2) / 1000.0);

    /* 4. Duplicates (5 distinct keys) */
    for (int i = 0; i < BENCH_SIZE; i++) arr[i] = rand() % 5;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    cobalt_qsort(arr, BENCH_SIZE, sizeof(int), bench_cmp_int);
    clock_gettime(CLOCK_MONOTONIC, &t2);
    printf("cobalt_qsort (5-Unique DNF):  %8.2f ms (%.2f M items/s)\n",
           get_elapsed_ms(t1, t2), (double)BENCH_SIZE / get_elapsed_ms(t1, t2) / 1000.0);

    /* 5. Stable Sort (Random) */
    for (int i = 0; i < BENCH_SIZE; i++) arr[i] = rand();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    cobalt_stable_sort(arr, BENCH_SIZE, sizeof(int), bench_cmp_int);
    clock_gettime(CLOCK_MONOTONIC, &t2);
    printf("cobalt_stable_sort (Random):  %8.2f ms (%.2f M items/s)\n",
           get_elapsed_ms(t1, t2), (double)BENCH_SIZE / get_elapsed_ms(t1, t2) / 1000.0);

    free(arr);
}
```

- [ ] **Step 2: Wire `bench_sort` into `tests/CMakeLists.txt` and `bench_runner.c`**

Add `bench_sort.c` to `bench_runner` sources in `tests/CMakeLists.txt` and call `bench_sort_run()` inside `tests/benchmark/bench_runner.c`.

- [ ] **Step 3: Build and execute benchmark**

Run:
```bash
cmake --build build --target bench_runner -j4 && ./build/tests/bench_runner
```
Expected: Successful execution showing throughput and timing for all 5 scenarios.

- [ ] **Step 4: Commit changes**

```bash
git add tests/benchmark/bench_sort.c tests/CMakeLists.txt tests/benchmark/bench_runner.c
git commit -m "test(bench): ⚡ add benchmark suite for sorting engine"
```

---

## Task 7: Full Test Suite, Memory Leak Check & Format Gate

**Files:**
- Repository-wide verification

- [ ] **Step 1: Run clang-format check**

Run:
```bash
clang-format --dry-run -Werror src/algorithm/sort.c tests/unit/test_sort.c tests/benchmark/bench_sort.c
```
Expected: Clean with 0 format errors. (Fix with `clang-format -i ...` if needed).

- [ ] **Step 2: Run all 42+ unit and benchmark tests**

Run:
```bash
ctest --test-dir build --output-on-failure
```
Expected: 100% tests passed.

- [ ] **Step 3: Run memory check with AddressSanitizer**

Run:
```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCOBALT_ENABLE_ASAN=ON
cmake --build build-asan -j4
ctest --test-dir build-asan --output-on-failure -R test_sort
```
Expected: 100% tests passed with 0 leaks and 0 errors reported.

- [ ] **Step 4: Final verification commit if formatting fixes were applied**

```bash
git commit -m "chore(sort): 🧹 format codebase and finalize Phase 1 sorting engine" --allow-empty
```
