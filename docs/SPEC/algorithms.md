# SPEC: Algorithms & Functional Streams (Layer 3)

**Module:** `include/cobalt/algorithm/`, `src/algorithm/`  
**Files:** sort.h, functional.h, sort.c, functional.c

## 1. Overview

Algorithm layer provides generic operations that work uniformly across all container implementations through their interface contracts. Algorithms do not depend on concrete types — only on the Sequence/Iterator interfaces.

## 2. Sorting

**Header:** `sort.h`

```c
// Comparison function type
typedef int (*compare_func_t)(const void *a, const void *b);

// Quick sort (array-based)
void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar);

// Insertion sort (small arrays)
void cobalt_insertion_sort(void *base, size_t nmemb, size_t size, compare_func_t compar);

// Merge sort (linked-list friendly)
void cobalt_list_sort(void **head, size_t *count, compare_func_t compar);
```

All sorting functions compare raw memory blocks using the provided comparator. The user is responsible for casting to appropriate types inside the comparator.

Example comparator for integer array:

```c
int cmp_int(const void *a, const void *b) {
    const int *ia = (const int *)a;
    const int *ib = (const int *)b;
    return (*ia > *ib) - (*ia < *ib);  // Safe three-way comparison
}
```

## 3. Functional Predicates

**Header:** `functional.h`

Common predicate functions for filtering and searching:

| Predicate | Description |
|-----------|-------------|
| `predicate_equal(a, b, comp)` | Checks equality using comparator |
| `predicate_not_equal(a, b, comp)` | Negation of equal |
| `predicate_null(item)` | Tests for NULL pointer |
| `predicate_nonnull(item)` | Tests for non-NULL pointer |

These compose well with higher-order functions like `COLL_FOREACH` or future `filter()` operators.

## 4. Stream Operators (Future)

Planned additions:
- `map(iterator, func)` — transform each element through a function
- `filter(iterator, pred)` — retain elements matching predicate
- `fold(iterator, init, acc_fn)` — reduce to single accumulator value
- `take_while(iterator, pred)` — emit while predicate true

These would form a lazy pipeline model inspired by C++ ranges or Rust iterators.

## 5. Algorithm Design Principles

- Operate purely through interface contracts (no knowledge of concrete storage)
- Zero dynamic allocation within algorithms themselves
- Comparator/functor callbacks passed as function pointers (no vtables needed)
- All algorithms O(n) unless specifically noted (sorting varies)
