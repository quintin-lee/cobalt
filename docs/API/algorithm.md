# API Reference — Algorithm Module (Layer 3)

**Module:** `include/cobalt/algorithm/`
**Dependencies:** `sort.h`, `functional.h`

## Overview

The algorithm module provides generic sorting, searching, and functional programming utilities. All functions operate on raw memory buffers (`void*` + `size_t` + `size_t element_size`) — no type coupling.

## Sorting

### Types

```c
typedef int (*compare_func_t)(const void *a, const void *b);
```
Returns < 0 if a < b, 0 if equal, > 0 if a > b (std::qsort convention).

### Functions

```c
void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar);
void cobalt_insertion_sort(void *base, size_t nmemb, size_t size, compare_func_t compar);
void cobalt_list_sort(void **head, size_t *count, compare_func_t compar);
```

| Function | Algorithm | Stable | Complexity | Notes |
|----------|-----------|--------|------------|-------|
| `cobalt_qsort` | Quick sort (stdlib) | No | O(n log n) avg | Unstable, in-place |
| `cobalt_insertion_sort` | Insertion sort | Yes | O(n²) | Efficient for small/nearly-sorted arrays |
| `cobalt_list_sort` | Merge sort | Yes | O(n log n) | Placeholder — not yet implemented |

**Usage example:**
```c
int values[] = {5, 2, 9, 1, 3};
cobalt_qsort(values, 5, sizeof(int), compar_int);
```

## Predicates

```c
int predicate_equal(const void *a, const void *b, compare_func_t comp);
int predicate_not_equal(const void *a, const void *b, compare_func_t comp);
int predicate_null(const void *item);
int predicate_nonnull(const void *item);
```

| Function | Returns true when |
|----------|-------------------|
| `predicate_equal` | `comp(a, b) == 0` (handles NULL safely) |
| `predicate_not_equal` | `comp(a, b) != 0` |
| `predicate_null` | `item == NULL` |
| `predicate_nonnull` | `item != NULL` |

## Search & Traversal

```c
void *cobalt_bsearch(const void *key, const void *base, size_t nmemb, size_t size, compare_func_t compar);
void *cobalt_find_if(const void *base, size_t nmemb, size_t size, predicate_func_t pred);
void  cobalt_for_each(const void *base, size_t nmemb, size_t size, operation_func_t op);
```

| Function | Complexity | Returns |
|----------|------------|---------|
| `cobalt_bsearch` | O(log n) | Pointer to matching element, or NULL |
| `cobalt_find_if` | O(n) | Pointer to first matching element, or NULL |
| `cobalt_for_each` | O(n) | void (modifies elements in-place) |

**Requirements:**
- `cobalt_bsearch`: array must be sorted according to `compar`
- `cobalt_find_if`/`cobalt_for_each`: array must contain `nmemb` elements of `size` bytes each

## Stream Operators

```c
typedef void (*map_func_t)(const void *item, void *output, void *user_data);
typedef void *(*fold_func_t)(void *accumulator, const void *item, void *user_data);
typedef int  (*predicate_func_t)(const void *item);
typedef void (*operation_func_t)(void *item);

int cobalt_map(const void *input, void *output, size_t nmemb, size_t size, map_func_t fn, void *user_data);
int cobalt_filter(const void *input, void *output, size_t *nmemb, size_t size, predicate_func_t pred);
void *cobalt_fold(const void *input, size_t nmemb, size_t size, void *initial, fold_func_t fn, void *user_data);
```

### cobalt_map
Transforms each element in-place from `input` to `output`. Both arrays must have capacity for `nmemb` elements of `size` bytes.
- Returns 0 on success, -1 on failure (NULL input/output, NULL fn)

### cobalt_filter
Copies elements matching `pred` from `input` to `output`. Output may be the same buffer as input (in-place filter).
- `nmemb` is updated to the new count on success
- Returns 0 on success, -1 on failure

### cobalt_fold
Accumulates all elements into a single result. `initial` is the starting accumulator value.
- Returns the accumulator pointer (may be same as `initial`)
- Returns `initial` unchanged if `nmemb == 0` or `fn == NULL`

**Usage example:**
```c
int src[] = {1, 2, 3, 4, 5};
int dst[5];
cobalt_map(src, dst, 5, sizeof(int), double_fn, NULL);
// dst = {2, 4, 6, 8, 10}

int even[5];
size_t n = 5;
cobalt_filter(src, even, &n, sizeof(int), is_even);
// even = {2, 4}, n = 2

int sum = 0;
cobalt_fold(src, 5, sizeof(int), &sum, add_fn, NULL);
// sum = 15
```
