# Cobalt High-Performance Sorting Engine Specification

- **Date:** 2026-10-16
- **Status:** Approved
- **Scope:** Layer 3 Algorithms (`src/algorithm/sort.c`, `include/cobalt/algorithm/sort.h`)

---

## 1. Overview & Goals

The Cobalt framework currently provides basic sorting primitives in `src/algorithm/sort.c`. However, analysis reveals several performance and algorithmic bottlenecks:
1. **Unstable Sort (`cobalt_qsort`)**: Directly forwards to standard C library `qsort()`, lacking control over pivot selection, duplicate handling, and platform-specific behaviors.
2. **Stable Sort (`cobalt_stable_sort`)**: Implemented as a naive insertion sort ($O(N^2)$), causing unacceptable quadratic degradation on arrays larger than a few hundred items.
3. **Partition Allocation (`partition_impl`)**: Allocates memory via `alloc->alloc` during partitioning, introducing significant allocation overhead in recursive inner loops.
4. **Insertion Sort Swap**: Relies on heap allocation for key element storage rather than utilizing stack buffers or in-place swapping.

### Primary Objectives
- Replace libc `qsort()` with a pure C11 **Pattern-Defeating Quicksort (PDQSort)** that guarantees $O(N \log N)$ worst-case time, $O(N)$ best-case time on pre-sorted or equal-key data, and zero heap allocation.
- Upgrade `cobalt_stable_sort()` to a true **Adaptive Run MergeSort (TimSort-lite)** achieving $O(N \log N)$ worst-case time, $O(N)$ best-case on natural runs, with 100% stability.
- Implement an **in-place word-aligned byte swap** routine (`cobalt_swap_bytes`) using CPU registers, eliminating dynamic allocation from swapping and partitioning loops.
- Maintain **100% backward compatibility** with all existing function prototypes in `include/cobalt/algorithm/sort.h` and pass all existing unit tests and allocator balance checks.

---

## 2. System Architecture

```
                               ┌────────────────────────────────┐
                               │  cobalt/algorithm/sort.h APIs  │
                               └───────┬────────────────┬───────┘
                                       │                │
                     ┌─────────────────┘                └─────────────────┐
                     ▼                                                    ▼
       ┌───────────────────────────┐                        ┌───────────────────────────┐
       │   Unstable Sort Engine    │                        │    Stable Sort Engine     │
       │       cobalt_qsort        │                        │    cobalt_stable_sort     │
       └─────────────┬─────────────┘                        └─────────────┬─────────────┘
                     │                                                    │
                     ▼                                                    ▼
       ┌───────────────────────────┐                        ┌───────────────────────────┐
       │          PDQSort          │                        │  Adaptive Run MergeSort   │
       │ - Tukey's Ninther Pivot   │                        │ - Natural Run Detection   │
       │ - 3-Way DNF Partitioning  │                        │ - In-Place Run Reversal   │
       │ - Introspective Heapsort  │                        │ - Run Stack Balancing     │
       │ - Small Array Insertion   │                        │ - Scratch-Buffer Merging  │
       └─────────────┬─────────────┘                        └─────────────┬─────────────┘
                     │                                                    │
                     └─────────────────┬──────────────────────────────────┘
                                       │
                                       ▼
                       ┌────────────────────────────────┐
                       │ In-Place Word Swap Subsystem   │
                       │ - 64-bit word swap (aligned)   │
                       │ - 32-bit word swap (aligned)   │
                       │ - Byte-by-byte tail swap       │
                       │ - Zero Heap Allocation         │
                       └────────────────────────────────┘
```

---

## 3. Detailed Component Designs

### 3.1 In-Place Word Swap Subsystem (`cobalt_swap_bytes`)

Generic sorting in C requires swapping two elements given pointers `char *a`, `char *b`, and `size_t size`. Instead of allocating temporary buffers on the heap:
1. **64-bit Aligned Fast Path**: When `((uintptr_t)a | (uintptr_t)b | size) & 7 == 0`, elements are swapped in chunks of `uint64_t`.
2. **32-bit Aligned Path**: When `((uintptr_t)a | (uintptr_t)b | size) & 3 == 0`, elements are swapped in chunks of `uint32_t`.
3. **Byte Path**: Remaining bytes or unaligned buffers are swapped byte-by-byte using `uint8_t`.

This routine operates purely within CPU registers without any heap or stack frame allocations.

### 3.2 High-Performance PDQSort Engine (`cobalt_qsort`)

The PDQSort engine combines the speed of randomized quicksort with deterministic pattern handling and guaranteed worst-case bounds.

#### 1. Thresholds & Small-Array Handling
- When $N \le 24$, switch to optimized in-place insertion sort.
- Small element keys ($size \le 128$) are buffered on a fixed stack buffer `char stack_key[128]` rather than heap allocation.

#### 2. Adaptive Pivot Selection
- **$N \le 128$**: Median-of-three among $\{base[0], base[N/2], base[N-1]\}$.
- **$N > 128$**: Tukey's Ninther (median of medians of three groups: head, middle, tail). This protects against pathological quadratic triggers (e.g., organ-pipe or sawtooth distributions).

#### 3. Dutch National Flag 3-Way Partitioning
- Partitions the array into three segments:
  $$\text{Elements} < \text{pivot} \quad\mid\quad \text{Elements} == \text{pivot} \quad\mid\quad \text{Elements} > \text{pivot}$$
- All elements equal to the pivot are placed in their final position and completely excluded from further recursive subdivisions.
- Arrays containing few unique keys (e.g., boolean flags, enums, duplicate records) sort in linear $O(N)$ time.

#### 4. Introspection & In-Place Heapsort Fallback
- An introspection recursion counter is initialized to $limit = 2 \times \lfloor\log_2 N\rfloor$.
- If recursion exceeds $limit$ due to adversarial inputs, the algorithm seamlessly transitions to **in-place Heapsort (`cobalt_heapsort`)**.
- `cobalt_heapsort` builds a binary max-heap and sifts down using `cobalt_swap_bytes`, strictly guaranteeing $O(N \log N)$ worst-case runtime and $O(1)$ auxiliary space.

---

### 3.3 Adaptive Run MergeSort Engine (`cobalt_stable_sort`)

`cobalt_stable_sort` preserves the exact relative ordering of equivalent elements while providing $O(N \log N)$ performance and $O(N)$ best-case adaptation.

#### 1. Natural Run Identification & In-Place Reversal
- Linear scanning identifies natural monotonic runs:
  - **Strictly Descending Run**: $A[i] > A[i+1] > \dots > A[j]$. Reversing this segment in-place in $O(j - i)$ preserves stability (since elements were strictly descending) and yields an ascending run.
  - **Weakly Ascending Run**: $A[i] \le A[i+1] \le \dots \le A[j]$. Left unmodified.
- If the first identified run covers the entire array of length $N$:
  - Pre-sorted arrays finish in $O(N)$ time with 0 swaps.
  - Reverse-sorted arrays finish in $O(N)$ time with in-place reversal.

#### 2. Short Run Extension
- If a detected run has length $< 32$, it is extended up to $\min(32, \text{remaining})$ using stable binary insertion sort.

#### 3. Run Stack & Merging Invariants
- Identified runs $(base, length)$ are pushed to a fixed run stack ($\le 64$ entries).
- Stack invariants ensure runs are balanced before merging:
  - $Run[i-2].len > Run[i-1].len + Run[i].len$
  - $Run[i-1].len > Run[i].len$
- When an invariant is broken, adjacent runs are merged.

#### 4. Scratch Buffer Management
- A single temporary buffer of size $\min(N/2, 65536) \times size$ is allocated at the root of `cobalt_stable_sort` (or requested from `cobalt_allocator_t *alloc` when provided).
- Merging copies the smaller of the two runs into the scratch buffer and performs a 2-way merge back into the target slice.
- If scratch memory allocation fails (OOM), the algorithm gracefully falls back to in-place insertion sort to guarantee error-free completion.

---

## 4. API Compatibility & Allocator Semantics

Existing APIs in `include/cobalt/algorithm/sort.h` remain 100% backward compatible:

```c
/* Unstable Sort */
void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar);

/* Insertion Sort */
void cobalt_insertion_sort(void *base, size_t nmemb, size_t size, compare_func_t compar);
void cobalt_insertion_sort_with_alloc(
    void *base, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc);

/* Stable Sort */
void cobalt_stable_sort(void *base, size_t nmemb, size_t size, compare_func_t compar);
void cobalt_stable_sort_with_alloc(
    void *base, size_t nmemb, size_t size, compare_func_t compar, cobalt_allocator_t *alloc);

/* Partitioning & Utilities */
size_t cobalt_partition(void *base, size_t nmemb, size_t size, const void *pivot, compare_func_t compar);
size_t cobalt_partition_with_alloc(void *base, size_t nmemb, size_t size, const void *pivot, compare_func_t compar, cobalt_allocator_t *alloc);
size_t cobalt_unique(void *base, size_t *nmemb, size_t size, compare_func_t compar);
void cobalt_list_sort(void **head, size_t *count, compare_func_t compar);
```

### Allocator Semantics
- In standard APIs (`cobalt_qsort`, `cobalt_stable_sort`, `cobalt_insertion_sort`), all small allocations are avoided via stack scratch buffers and word swaps.
- In `*_with_alloc` APIs, if an explicit `cobalt_allocator_t *alloc` is provided:
  - `cobalt_insertion_sort_with_alloc`: Preserves explicit key allocation via `alloc->alloc` and `alloc->free` to satisfy existing test assertions (`mock_sort_alloc_count > 0`).
  - `cobalt_stable_sort_with_alloc`: Uses `alloc->alloc` for the merge scratch buffer and frees it upon completion.
  - `cobalt_partition_with_alloc`: Performs partition with allocator tracking if specified.

---

## 5. Verification & Benchmark Strategy

### 5.1 Unit Testing (`tests/unit/test_sort.c`)
1. **Stability Verification**: Array of 5,000 composite structures with `(key, original_order)`. After sorting, identical keys must maintain strictly increasing `original_order`.
2. **Pathological Distributions**:
   - All identical elements ($O(N)$ Dutch National Flag check).
   - Strictly pre-sorted ascending ($O(N)$ check).
   - Strictly pre-sorted descending ($O(N)$ check).
   - Sawtooth / organ-pipe distribution (introspective Heapsort fallback check).
3. **Allocator Tracking**: Validate zero memory leaks under ASan/UBSan/Valgrind.

### 5.2 Benchmark Suite (`tests/benchmark/bench_sort.c`)
- Benchmark random, sorted, reverse, and duplicate datasets ($10^5$ elements).
- Compare execution time (ms) and throughput (items/sec) against libc `qsort` and prior implementation.
