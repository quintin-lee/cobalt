# SPEC: Algorithms & Functional Streams (Layer 3)

**Module:** `include/cobalt/algorithm/`, `src/algorithm/`  
**Files:** `sort.h`, `functional.h`, `sort.c`, `functional.c`

## 1. Overview

The Algorithm layer provides generic operations that operate uniformly across raw memory buffers and collection abstractions. Algorithms are completely decoupled from concrete container layouts, operating either via raw memory element pointers (`void *base, size_t nmemb, size_t size, compare_func_t compar`) or generic iterator/sequence protocols.

As of Cobalt v2.5.0, the sorting subsystem has been re-engineered into an industrial-grade, zero-dependency engine combining:
- In-place multi-word memory swap without heap allocations
- Pattern-Defeating Quicksort (PDQSort) with Dutch National Flag 3-way partitioning
- Deterministic in-place Heapsort fallback
- Adaptive Run MergeSort (TimSort-lite) guaranteeing 100% stable sorting
- Zero-allocation Hoare partitioning

---

## 2. High-Performance Sorting Engine

### 2.1 In-Place Multi-Word Swap (`cobalt_swap_bytes`)

Element swapping in generic C algorithms traditionally incurs heap allocations or repeated single-byte copies. Cobalt implements an in-place word-aligned swap routine:

```c
static inline void cobalt_swap_bytes(void *a, void *b, size_t size)
```

- **64-bit Fast Path**: When pointers and remaining sizes are 8-byte aligned, swaps using native 64-bit words (`uint64_t`).
- **32-bit Fast Path**: When 4-byte aligned, swaps using native 32-bit words (`uint32_t`).
- **Tail Path**: Any remaining bytes (< 4) are swapped byte-by-byte (`uint8_t`).
- **Zero Allocations**: Pure stack register operations with zero heap memory overhead.

### 2.2 Pattern-Defeating Quicksort (`cobalt_qsort`)

`cobalt_qsort` implements pure C11 Pattern-Defeating Quicksort (PDQSort), combining the average-case speed of randomized quicksort with deterministic worst-case bounds and linear time on structured inputs.

#### Pivot Selection
- **$N \le 128$**: Median-of-3 (`pdq_median3`) sampling the first, middle, and last elements.
- **$N > 128$**: Tukey's ninther (`pdq_ninther`), computing the pseudo-median of three medians-of-three, ensuring robust resistance against adversarial patterns.

#### Dutch National Flag (DNF) 3-Way Partitioning
Partitions the slice into three distinct regions:
1. Elements strictly less than pivot ($< \text{pivot}$)
2. Elements equal to pivot ($== \text{pivot}$)
3. Elements strictly greater than pivot ($> \text{pivot}$)

```
[  < pivot  |  == pivot  |  > pivot  ]
```
This guarantees optimal $O(N)$ linear time on inputs with low cardinality or massive duplicate keys.

#### Small-Array Cutoff
Sub-arrays where $N \le 24$ immediately terminate recursion and are sorted via an optimized, stack-buffered insertion sort (`insertion_sort_impl`), eliminating recursion overhead on tiny partitions.

#### Introspective Heapsort Fallback
To prevent $O(N^2)$ degradation from quadratic patterns (e.g. killer adversarial sequences):
- A recursion allowance counter is initialized to $2 \lfloor \log_2 N \rfloor$.
- If a partition split is severely unbalanced (left or right child size $< N / 8$), the allowance is decremented.
- When the allowance reaches zero, the engine automatically switches to `cobalt_heapsort_impl`, guaranteeing deterministic $O(N \log N)$ worst-case execution.

#### Tail-Call Recursion Optimization
The engine always recurses into the smaller partition first and loops on the larger partition. This bounds maximum execution stack depth strictly to $O(\log N)$ frames.

---

### 2.3 Pure C11 Introspective Heapsort (`cobalt_heapsort`)

An in-place, zero-allocation Heapsort implementation:

```c
void cobalt_heapsort(void *base, size_t nmemb, size_t size, compare_func_t compar);
```

- **In-Place Max-Heap Construction**: Sifts down internal nodes starting from index $\lfloor N / 2 \rfloor - 1$ down to 0.
- **Overflow-Proof Index Arithmetic**: Sift-down condition uses `root < count / 2` to prevent integer overflow on large element counts.
- **Sorting Phase**: Repeatedly swaps root with the active boundary and restores heap invariant using `cobalt_swap_bytes`.
- **Characteristics**: $O(N \log N)$ worst-case and average-case time, $O(1)$ auxiliary memory, zero dynamic allocations.

---

### 2.4 Adaptive Run MergeSort / TimSort-lite (`cobalt_stable_sort`)

`cobalt_stable_sort` and `cobalt_stable_sort_with_alloc` provide guaranteed stable sorting with adaptive linear-time performance on pre-sorted and partially-sorted data.

#### Natural Run Detection & In-Place Reversal
- Linear scan identifies strictly descending runs (`compar(a, b) > 0`) and reverses them in-place via `cobalt_swap_bytes`.
- Identifies non-descending runs (`compar(a, b) <= 0`) and preserves them intact.
- Short runs ($< 16$ elements) are extended up to $\min(16, \text{remaining})$ using insertion sort.

#### Bounded Scratch Buffer & Bidirectional Merging
- Scratch buffer sized strictly to $\lceil N / 2 \rceil$ elements.
- When merging adjacent runs $A$ and $B$:
  - If $|A| \le |B|$: runs `merge_lo`, copying $A$ to scratch and merging low-to-high into the slice.
  - If $|A| > |B|$: runs `merge_hi`, copying $B$ to scratch and merging high-to-low into the slice.
- Scratch memory overhead is strictly bounded to at most $50\%$ of array size.

#### Guaranteed 100% Stability
During low-to-high merge, when elements are equal (`compar(scratch, b) <= 0`), the element from the left run ($A$) is strictly preferred:
```c
if (compar(scratch + i * size, b + j * size) <= 0) {
    memcpy(dest, scratch + i * size, size); // Left run preserved first
    i++;
} else {
    memcpy(dest, b + j * size, size);
    j++;
}
```

#### OOM Fallback & Allocator Injection
If scratch allocation fails under extreme memory pressure, the algorithm degrades gracefully to stack-buffered insertion sort without crashing. Custom allocators can be injected via `cobalt_stable_sort_with_alloc`.

---

### 2.5 Zero-Allocation Hoare Partitioning (`cobalt_partition`)

```c
size_t cobalt_partition(void *base, size_t nmemb, size_t size, const void *pivot, compare_func_t compar);
size_t cobalt_partition_with_alloc(void *base, size_t nmemb, size_t size, const void *pivot, compare_func_t compar, cobalt_allocator_t *alloc);
```

- Partitions array into elements strictly less than `pivot` followed by elements greater than or equal to `pivot`.
- Default `cobalt_partition` performs zero dynamic memory allocations, swapping elements in-place using `cobalt_swap_bytes`.
- `cobalt_partition_with_alloc` preserves allocator lifetime tracking for specialized instrumentation environments.
- Returns the index of the first element in the greater-or-equal partition.

---

### 2.6 Range Deduplication (`cobalt_unique`)

```c
size_t cobalt_unique(void *base, size_t *nmemb, size_t size, compare_func_t compar);
```

- Performs an in-place linear $O(N)$ pass over a pre-sorted array, compacting consecutive duplicate elements.
- Updates `*nmemb` and returns the new logical element count.

---

## 3. Search & Traversal Utilities

| Function | Complexity | Description |
|----------|------------|-------------|
| `cobalt_bsearch(key, base, nmemb, size, compar)` | $O(\log N)$ | Standard binary search on sorted range, returning pointer to match or `NULL`. |
| `cobalt_find_if(base, nmemb, size, pred)` | $O(N)$ | Linear predicate scan returning pointer to first match or `NULL`. |
| `cobalt_for_each(base, nmemb, size, op)` | $O(N)$ | Invokes unary operation callback `op` across all elements in order. |

---

## 4. Functional Stream Operators

Functional stream operators enable composable pipeline transformations on contiguous element buffers:

```c
typedef void (*map_func_t)(const void *item, void *output, void *user_data);
typedef int  (*predicate_func_t)(const void *item);
typedef void *(*fold_func_t)(void *accumulator, const void *item, void *user_data);

int   cobalt_map(const void *input, void *output, size_t nmemb, size_t size, map_func_t fn, void *user_data);
int   cobalt_filter(const void *input, void *output, size_t *nmemb, size_t size, predicate_func_t pred);
void *cobalt_fold(const void *input, size_t nmemb, size_t size, void *initial, fold_func_t fn, void *user_data);
```

- **`cobalt_map`**: Transforms each element from `input` into `output` using `fn`. Supports in-place transformation when `input == output`.
- **`cobalt_filter`**: Evaluates `pred` on each element; matching elements are copied to `output`. Supports in-place filtering. Updates `*nmemb` to retained count.
- **`cobalt_fold`**: Reductive fold (left accumulator) aggregating all elements into `initial`.

---

## 5. Functional Predicates

Common predicate functions defined in `functional.h`:

| Predicate | Condition |
|-----------|-----------|
| `predicate_equal(a, b, comp)` | Returns 1 if `comp(a, b) == 0`, handling NULL pointers safely. |
| `predicate_not_equal(a, b, comp)` | Returns 1 if `comp(a, b) != 0`. |
| `predicate_null(item)` | Returns 1 if `item == NULL`. |
| `predicate_nonnull(item)` | Returns 1 if `item != NULL`. |

---

## 6. Algorithm Complexity Matrix

| Algorithm | Function | Best Time | Average Time | Worst Time | Auxiliary Memory | Stability |
|-----------|----------|-----------|--------------|------------|------------------|-----------|
| **PDQSort** | `cobalt_qsort` | $O(N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(\log N)$ stack | Unstable |
| **Heapsort** | `cobalt_heapsort` | $O(N \log N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(1)$ | Unstable |
| **Adaptive MergeSort** | `cobalt_stable_sort` | $O(N)$ | $O(N \log N)$ | $O(N \log N)$ | $O(N / 2)$ heap | **Stable** |
| **Insertion Sort** | `cobalt_insertion_sort` | $O(N)$ | $O(N^2)$ | $O(N^2)$ | $O(1)$ stack | **Stable** |
| **Hoare Partition** | `cobalt_partition` | $O(N)$ | $O(N)$ | $O(N)$ | $O(1)$ | Unstable |
| **Deduplication** | `cobalt_unique` | $O(N)$ | $O(N)$ | $O(N)$ | $O(1)$ | Stable |
| **Binary Search** | `cobalt_bsearch` | $O(1)$ | $O(\log N)$ | $O(\log N)$ | $O(1)$ | N/A |
| **Linear Search** | `cobalt_find_if` | $O(1)$ | $O(N)$ | $O(N)$ | $O(1)$ | N/A |
| **Stream Map** | `cobalt_map` | $O(N)$ | $O(N)$ | $O(N)$ | $O(1)$ | Stable |
| **Stream Filter** | `cobalt_filter` | $O(N)$ | $O(N)$ | $O(N)$ | $O(1)$ | Stable |
| **Stream Fold** | `cobalt_fold` | $O(N)$ | $O(N)$ | $O(N)$ | $O(1)$ | N/A |
