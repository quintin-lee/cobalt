# API Reference — Algorithm Module (Layer 3)

**Module:** `include/cobalt/algorithm/`  
**Headers:** `sort.h`, `functional.h`

## Overview

The Algorithm module provides generic sorting, partitioning, searching, and functional stream transformations. All functions operate directly on raw memory buffers (`void *` + element count + element size) or functional predicates — ensuring zero coupling to concrete container implementations.

---

## Sorting & Partitioning (`sort.h`)

### Types

```c
typedef int (*compare_func_t)(const void *a, const void *b);
```
Returns an integer representing comparison order:
- `< 0` if `a < b`
- `0` if `a == b`
- `> 0` if `a > b`

### Functions

```c
/* Array-based sorting */
void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar);
void cobalt_heapsort(void *base, size_t nmemb, size_t size, compare_func_t compar);
void cobalt_insertion_sort(void *base, size_t nmemb, size_t size, compare_func_t compar);
void cobalt_insertion_sort_with_alloc(void *base, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc);
void cobalt_stable_sort(void *base, size_t nmemb, size_t size, compare_func_t compar);
void cobalt_stable_sort_with_alloc(void *base, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc);

/* Partitioning and deduplication */
size_t cobalt_partition(void *base, size_t nmemb, size_t size, const void *pivot, compare_func_t compar);
size_t cobalt_partition_with_alloc(void *base, size_t nmemb, size_t size, const void *pivot, compare_func_t compar, cobalt_allocator_t *alloc);
size_t cobalt_unique(void *base, size_t *nmemb, size_t size, compare_func_t compar);

/* Linked list-based sorting */
void cobalt_list_sort(void **head, size_t *count, compare_func_t compar);
```

### Sorting Characteristics

| Function | Algorithm | Stable | Best Time | Avg Time | Worst Time | Aux Memory | Notes |
|:---|:---|:---:|:---:|:---:|:---:|:---:|:---|
| `cobalt_qsort` | PDQSort | No | $O(N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(\log N)$ stack | DNF 3-way partition, ninther pivot, Heapsort fallback |
| `cobalt_heapsort` | Heapsort | No | $O(N \log N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(1)$ | In-place max-heap, zero heap allocation |
| `cobalt_stable_sort` | TimSort-lite | **Yes** | $O(N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(N / 2)$ heap | Natural run detection, bidirectional scratch merge |
| `cobalt_insertion_sort` | Insertion Sort | **Yes** | $O(N)$ | $O(N^2)$ | $O(N^2)$ | $O(1)$ stack | Stack-buffered, optimal for $N \le 24$ |
| `cobalt_partition` | Hoare Partition | No | $O(N)$ | $O(N)$ | $O(N)$ | $O(1)$ | In-place word swap, zero heap allocation |
| `cobalt_unique` | Deduplication | **Yes** | $O(N)$ | $O(N)$ | $O(N)$ | $O(1)$ | In-place compaction of sorted ranges |
| `cobalt_list_sort` | Merge Sort | **Yes** | $O(N \log N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(\log N)$ | In-place node pointer manipulation |

---

### Detailed Sorting Operations

#### `cobalt_qsort`
```c
void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar);
```
Sorts an array in-place using Pattern-Defeating Quicksort.
- Unstable sort.
- Utilizes Tukey's ninther pseudo-median when $nmemb > 128$, and median-of-3 otherwise.
- Uses Dutch National Flag 3-way partition to achieve linear $O(N)$ time on duplicate-heavy inputs.
- Cuts off to stack-buffered insertion sort when partition size $\le 24$.
- Automatically falls back to `cobalt_heapsort` if recursion depth exceeds $2 \lfloor \log_2 nmemb \rfloor$ under unbalanced splits, guaranteeing $O(N \log N)$ worst-case execution.

#### `cobalt_heapsort`
```c
void cobalt_heapsort(void *base, size_t nmemb, size_t size, compare_func_t compar);
```
Sorts an array in-place using introspective max-heapsort.
- Unstable sort.
- Zero dynamic memory allocations.
- Deterministic $O(N \log N)$ time complexity and $O(1)$ auxiliary space.

#### `cobalt_stable_sort` / `cobalt_stable_sort_with_alloc`
```c
void cobalt_stable_sort(void *base, size_t nmemb, size_t size, compare_func_t compar);
void cobalt_stable_sort_with_alloc(void *base, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc);
```
Sorts an array while strictly preserving the relative order of equivalent elements.
- Guaranteed 100% stable sort.
- Automatically identifies natural non-descending runs and strictly descending runs (reversing descending runs in-place via word swap).
- Merges runs using a bounded scratch buffer ($\le \lceil N / 2 \rceil$ elements) with bidirectional `merge_lo` and `merge_hi`.
- Degrades gracefully to stack-buffered insertion sort if memory allocation fails.

#### `cobalt_partition` / `cobalt_partition_with_alloc`
```c
size_t cobalt_partition(void *base, size_t nmemb, size_t size, const void *pivot, compare_func_t compar);
size_t cobalt_partition_with_alloc(void *base, size_t nmemb, size_t size, const void *pivot, compare_func_t compar, cobalt_allocator_t *alloc);
```
Partitions an array around a pivot value (Hoare-style).
- All elements `< pivot` precede the returned boundary index.
- All elements `>= pivot` are placed at or after the returned index.
- Passing `alloc == NULL` (or calling `cobalt_partition`) performs zero dynamic memory allocations, using in-place multi-word swaps.

#### `cobalt_unique`
```c
size_t cobalt_unique(void *base, size_t *nmemb, size_t size, compare_func_t compar);
```
Removes consecutive duplicate elements from a pre-sorted range.
- Compacts unique elements to the front of the array.
- Updates `*nmemb` and returns the new count.

---

## Predicates & Search (`functional.h`)

### Predicate Helpers

```c
int predicate_equal(const void *a, const void *b, compare_func_t comp);
int predicate_not_equal(const void *a, const void *b, compare_func_t comp);
int predicate_null(const void *item);
int predicate_nonnull(const void *item);
```

| Function | Returns true (1) when |
|----------|-----------------------|
| `predicate_equal` | `comp(a, b) == 0` (safely handles NULL pointers) |
| `predicate_not_equal` | `comp(a, b) != 0` |
| `predicate_null` | `item == NULL` |
| `predicate_nonnull` | `item != NULL` |

### Search & Traversal

```c
void *cobalt_bsearch(const void *key, const void *base, size_t nmemb, size_t size, compare_func_t compar);
void *cobalt_find_if(const void *base, size_t nmemb, size_t size, predicate_func_t pred);
void  cobalt_for_each(const void *base, size_t nmemb, size_t size, operation_func_t op);
```

| Function | Complexity | Description |
|----------|------------|-------------|
| `cobalt_bsearch` | $O(\log N)$ | Binary search over sorted range. Returns matching pointer or `NULL`. |
| `cobalt_find_if` | $O(N)$ | Linear scan. Returns pointer to first element satisfying `pred`, or `NULL`. |
| `cobalt_for_each` | $O(N)$ | Invokes `op(element)` sequentially for every element in array. |

---

## Stream Operators (`functional.h`)

```c
typedef void (*map_func_t)(const void *item, void *output, void *user_data);
typedef void *(*fold_func_t)(void *accumulator, const void *item, void *user_data);
typedef int  (*predicate_func_t)(const void *item);
typedef void (*operation_func_t)(void *item);

int   cobalt_map(const void *input, void *output, size_t nmemb, size_t size, map_func_t fn, void *user_data);
int   cobalt_filter(const void *input, void *output, size_t *nmemb, size_t size, predicate_func_t pred);
void *cobalt_fold(const void *input, size_t nmemb, size_t size, void *initial, fold_func_t fn, void *user_data);
```

### `cobalt_map`
Transforms elements from `input` into `output` using `fn`.
- `input` and `output` may point to the same buffer for in-place transformation.
- Returns 0 on success, -1 on invalid arguments.

### `cobalt_filter`
Copies elements from `input` to `output` for which `pred(item)` returns non-zero.
- Supports in-place filtering (`input == output`).
- Updates `*nmemb` with the number of retained elements.
- Returns 0 on success, -1 on invalid arguments.

### `cobalt_fold`
Aggregates all elements into a single accumulator starting from `initial`.
- Returns the resulting accumulator pointer.
- Returns `initial` untouched if `nmemb == 0` or `fn == NULL`.

---

## Regular Expression Engine (`regex.h`)

Cobalt provides a pure C11 deterministic Pike VM regular expression engine with linear $O(N \times M)$ runtime, ensuring mathematical immunity against Regular Expression Denial of Service (ReDoS) catastrophic backtracking attacks.

### Types & Constants

```c
#define COBALT_REGEX_MAX_CAPTURES 16

typedef enum {
    COBALT_REGEX_DEFAULT          = 0,
    COBALT_REGEX_CASE_INSENSITIVE = 1 << 0,
    COBALT_REGEX_DOTALL           = 1 << 1,
    COBALT_REGEX_MULTILINE        = 1 << 2
} cobalt_regex_flags_t;

typedef struct {
    size_t start;
    size_t end;
    size_t length;
} cobalt_regex_capture_t;

typedef struct cobalt_regex cobalt_regex_t;
```

### Functions

```c
/* Compilation & Lifecycle */
cobalt_regex_t *cobalt_regex_compile(const char *pattern, int flags);
cobalt_regex_t *cobalt_regex_compile_with_alloc(const char *pattern, int flags, cobalt_allocator_t *alloc);
void cobalt_regex_destroy(cobalt_regex_t *re);

/* Matching & Search */
int cobalt_regex_search(const cobalt_regex_t *re, const char *text, cobalt_regex_capture_t *captures, size_t max_captures);
int cobalt_regex_match_exact(const cobalt_regex_t *re, const char *text, cobalt_regex_capture_t *captures, size_t max_captures);
int cobalt_regex_test(const char *pattern, const char *text);

/* Inspection */
size_t cobalt_regex_capture_count(const cobalt_regex_t *re);
```

### Supported Syntax

- **Literals & Escapes**: `\n`, `\r`, `\t`, `\v`, `\f`, `\0`, escaped metacharacters `\\`, `\.`, `\*`, etc.
- **Character Classes**: `[...]`, negated `[^...]`, ranges `a-z`, shorthands `\d`, `\D`, `\w`, `\W`, `\s`, `\S` (single-cycle 256-bit bitmap lookup).
- **Wildcard**: `.` (matches all except `\n`, or all including `\n` under `COBALT_REGEX_DOTALL`).
- **Anchors**: `^` (line start), `$` (line end), `\b` (word boundary), `\B` (non-word boundary).
- **Quantifiers**: `*`, `+`, `?`, `{m,n}`, `{m}`, `{m,}` in both greedy and non-greedy (`*?`, `+?`, `??`, `{m,n}?`) modes.
- **Alternation**: `A | B | C` with deterministic left-to-right priority.
- **Captures & Groups**: `(...)` up to 15 sub-groups, non-capturing `(?:...)`.
- **Backreferences**: `\1` through `\15` evaluated in lockstep without backtracking.

---

## Usage Example

```c
#include <cobalt/algorithm/sort.h>
#include <cobalt/algorithm/functional.h>
#include <cobalt/algorithm/regex.h>
#include <stdio.h>

static int cmp_ints(const void *a, const void *b) {
    int va = *(const int *)a;
    int vb = *(const int *)b;
    return (va > vb) - (va < vb);
}

int main(void) {
    // 1. Regular expression parsing & capture extraction
    cobalt_regex_t *re = cobalt_regex_compile("(\\d{4})-(\\d{2})-(\\d{2})", COBALT_REGEX_DEFAULT);
    cobalt_regex_capture_t caps[4];
    if (cobalt_regex_search(re, "Build date: 2026-10-17", caps, 4)) {
        printf("Matched date: %.*s\n", (int)caps[0].length, "Build date: 2026-10-17" + caps[0].start);
    }
    cobalt_regex_destroy(re);

    // 2. High-performance PDQSort
    int data[] = {42, 17, 88, 17, 95, 3, 1024, 7};
    size_t count = sizeof(data) / sizeof(data[0]);
    cobalt_qsort(data, count, sizeof(int), cmp_ints);

    // 3. In-place deduplication
    size_t unique_count = cobalt_unique(data, &count, sizeof(int), cmp_ints);
    printf("Unique count: %zu\n", unique_count);

    return 0;
}
```
