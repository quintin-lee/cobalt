# Cobalt Phase 2: Code Quality & Completeness Optimization

## Overview

Phase 2 focuses on fixing critical bugs, eliminating code duplication, unifying error handling conventions, and completing missing functionality. This phase is about maturing the codebase to production quality — not adding new features.

## Scope

| Category | Items |
|----------|-------|
| Bug Fixes | 3 critical bugs in list.c, 2 stub implementations |
| Code Deduplication | Consolidate `my_strdup` into shared utility |
| API Unification | Standardize error handling and return conventions |
| Missing Features | Implement list iterator, list pop_back, list get, vector remove |
| Build/CI | Add Valgrind/ASan checks, performance benchmarks |
| Documentation | Remove duplicate examples, fix header include order |

## Design

### 1. Critical Bug Fixes

#### 1.1 Fix `list.c:18` — Compilation Error

**Problem:** `cobalt_list_size()` body contains `return item == NULL;` which is a copy-paste artifact from `functional.c`. This causes a compilation error (`item` undeclared).

**Fix:**
```c
size_t cobalt_list_size(cobalt_list_t* list)
{
    return list ? ((cobalt_list_impl_t*)list)->size : 0;
}
```

#### 1.2 Implement `list_iterator_seq()`

**Problem:** Returns NULL unconditionally. Breaks all Sequence-based iteration over lists.

**Fix:** Implement a proper linked-list iterator in `interface/iterator.c` or create a list-specific iterator. Since `cobalt_iterator_t` is generic, add a list-aware factory:

```c
static cobalt_iterator_t* list_iterator_seq(cobalt_sequence_t* self)
{
    cobalt_list_impl_t* list = (cobalt_list_impl_t*)self;
    return cobalt_list_iterator_create(list);
}
```

Add `cobalt_list_iterator_create()` and `cobalt_list_iterator_destroy()` to `list.h`/`list.c`.

#### 1.3 Implement `list_pop_back()`

**Problem:** Returns NULL without doing anything. The doubly-linked list structure has a `tail` pointer — this is trivial to implement.

**Fix:**
```c
void* cobalt_list_pop_back(cobalt_list_t* list)
{
    if (!list || !((cobalt_list_impl_t*)list)->tail)
        return NULL;
    cobalt_list_impl_t* impl = (cobalt_list_impl_t*)list;
    list_node_t* node = impl->tail;
    void* data = node->data;
    impl->tail = node->prev;
    if (impl->tail)
        impl->tail->next = NULL;
    else
        impl->head = NULL;
    free(node);
    impl->size--;
    return data;
}
```

#### 1.4 Implement `list_get()`

**Problem:** Returns NULL unconditionally. For a doubly-linked list, O(n/2) traversal from the nearer end is acceptable.

**Fix:**
```c
void* cobalt_list_get(cobalt_list_t* list, size_t index)
{
    if (!list) return NULL;
    cobalt_list_impl_t* impl = (cobalt_list_impl_t*)list;
    if (index >= impl->size) return NULL;
    list_node_t* node;
    if (index < impl->size / 2) {
        node = impl->head;
        for (size_t i = 0; i < index; i++)
            node = node->next;
    } else {
        node = impl->tail;
        for (size_t i = impl->size - 1; i > index; i--)
            node = node->prev;
    }
    return node->data;
}
```

#### 1.5 Implement `vector_remove_seq()`

**Problem:** Empty stub in the Sequence interface implementation.

**Fix:** Find element by pointer equality, shift remaining elements, update size.

```c
static void vector_remove_seq(cobalt_sequence_t* self, void* item)
{
    cobalt_vector_impl_t* vec = (cobalt_vector_impl_t*)self;
    for (size_t i = 0; i < vec->size; i++) {
        if (vec->items[i] == item) {
            memmove(vec->items + i, vec->items + i + 1,
                    (vec->size - i - 1) * sizeof(void*));
            vec->size--;
            return;
        }
    }
}
```

### 2. Code Deduplication — `my_strdup`

**Problem:** `my_strdup` is reimplemented identically in 4 source files:
- `src/container/hashmap.c:7`
- `src/container/treemap.c:6`
- `src/module/json.c:9`
- `src/core/class.c:6`

**Fix:** Move to a shared utility header `include/cobalt/utils/string.h` with the implementation in a new `src/utils/string.c`.

```c
// include/cobalt/utils/string.h
#ifndef COBALT_STRING_UTIL_H
#define COBALT_STRING_UTIL_H

#include <stddef.h>

/**
 * @brief Portable strdup for C11 (no strdup guarantee in C11)
 * @param s Source string (may be NULL)
 * @return Duplicate string or NULL on failure
 */
char* cobalt_strdup(const char* s);

#endif
```

All 4 files replace `my_strdup` with `#include "cobalt/utils/string.h"` + `cobalt_strdup()`.

### 3. API Error Handling Unification

**Problem:** Inconsistent return conventions across containers:

| Function | Return on Success | Return on Failure | Sets errno? |
|----------|-------------------|-------------------|-------------|
| `hashmap_put` | 0 | -1 | Yes |
| `hashmap_get` | void* | NULL | Yes |
| `hashmap_remove` | 0 | -1 | Yes |
| `vector_push` | 0 | -1 | **No** |
| `vector_set` | 0 | -1 | **No** |
| `list_push_front` | 0 | -1 | **No** |
| `stack_push` | 0 | -1 | **No** |
| `queue_enqueue` | 0 | -1 | **No** |

**Design:** Add optional `cobalt_error_t* err` parameter to all mutating functions. When non-NULL, the function sets the error code. Existing callers without the parameter continue to work (backward compatible).

```c
// Updated signatures (backward compatible - err is new optional parameter)
int cobalt_vector_push(cobalt_vector_t* vec, void* item, cobalt_error_t* err);
int cobalt_vector_set(cobalt_vector_t* vec, size_t index, void* item, cobalt_error_t* err);
int cobalt_hashmap_put(cobalt_hashmap_t* map, const char* key, void* value, cobalt_error_t* err);
// ... etc for all containers

// Existing callers unaffected:
cobalt_vector_push(vec, &value);  // err = NULL, same behavior

// New callers can check errors:
cobalt_error_t err = COBALT_SUCCESS;
cobalt_vector_push(vec, &value, &err);
if (err != COBALT_SUCCESS) { /* handle */ }
```

**Note:** This is a soft change — the new `err` parameter is added at the END of existing signatures, so it is NOT backward compatible at the C level. Alternative: keep existing signatures unchanged and rely on `cobalt_error_get_current()` which is already implemented. This is simpler and truly zero-breaking.

**Revised Design (Zero-Breaking):** Keep existing signatures. Ensure ALL functions that can fail call `cobalt_error_set()` with the appropriate code. This means:
- Add `cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY)` to every OOM path in vector, list, stack, queue
- Add `cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT)` to every null-pointer check in vector, list, stack, queue
- This makes the error system actually usable without API changes

### 4. HashMap Auto-Resize (Complete Phase 1)

**Problem:** Phase 1 design specifies auto-resize but the code already has a partial implementation (`hashmap_ensure_buckets`). Verify it's working correctly and add tests.

**Status Check:** The code in `hashmap.c:78-109` already implements `hashmap_ensure_buckets()` with 2× growth. The resize triggers at load factor > 0.75 (line 146: `(impl->size + 1) * 4 / impl->bucket_count > 3`). **This is already implemented.** Add comprehensive resize tests.

### 5. CI/CD Improvements

#### 5.1 Add Valgrind Memory Check

```yaml
# In .github/workflows/ci.yml, add to build job:
- name: Run Valgrind memory check
  run: |
    valgrind --leak-check=full --error-exitcode=1 \
      ./build/tests/cobalt_test
```

#### 5.2 Add AddressSanitizer Build

```yaml
- name: Build with ASan
  run: |
    cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-fsanitize=address -g" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
    cmake --build build-asan --parallel
    ctest --test-dir build-asan --output-on-failure
```

#### 5.3 Add Performance Benchmarks

Create `tests/benchmark/` directory with micro-benchmarks:
- HashMap: insert 10K/100K strings, measure get latency
- Vector: push 1M items, measure resize overhead
- EventLoop: 1000 timers, measure tick latency
- Compare against Phase 1 baseline

### 6. Documentation Cleanup

#### 6.1 Remove Duplicate Examples

`docs/EXAMPLES/examples/` contains copies of files also in `examples/`. Keep only `examples/` as the canonical source. The docs copies add no value and create maintenance burden (two places to sync).

#### 6.2 Fix Duplicate `compare_func_t` Typedef

**Problem:** `compare_func_t` is typedef'd identically in both `include/cobalt/algorithm/sort.h:12` and `include/cobalt/algorithm/functional.h:12`. This is a duplicate definition that works only because both headers are included before any compilation unit uses them (relying on include guards to prevent actual redefinition errors). It's fragile and violates DRY.

**Fix:** Keep the definition in `sort.h` (authoritative location). Remove it from `functional.h` and add `#include "cobalt/algorithm/sort.h"` to `functional.h` instead. Then fix `cobalt.h` include order so `sort.h` is included before `functional.h`.

```c
// include/cobalt/algorithm/functional.h
// REMOVE: typedef int (*compare_func_t)(const void* a, const void* b);
// ADD:   #include "cobalt/algorithm/sort.h"
```

```c
// include/cobalt/cobalt.h
#include <cobalt/algorithm/sort.h>      // define compare_func_t first
#include <cobalt/algorithm/functional.h> // uses compare_func_t
```

### 7. Red-Black Tree Balance (Treemap)

**Problem:** `treemap.c` uses a standard BST, not a Red-Black tree as the header and spec claim. This means worst-case O(n) lookups on sorted input.

**Fix:** Implement proper RB-tree insert with rotations and color fixes. This is a significant change — scope as optional Phase 2b.

## Implementation Phases

### Phase 2a: Critical Fixes (Priority: Must Do)
1. Fix `list.c` compilation error
2. Implement `list_pop_back()`, `list_get()`, `list_iterator_seq()`
3. Implement `vector_remove_seq()`
4. Fix `cobalt.h` include order
5. Unify `my_strdup` into shared utility

### Phase 2b: Error Handling & Quality (Priority: Should Do)
1. Add `cobalt_error_set()` to all failure paths in containers
2. Add Valgrind and ASan to CI
3. Fix Treemap to be a real Red-Black tree
4. Add HashMap resize stress tests

### Phase 2c: Polish (Priority: Nice to Have)
1. Add performance benchmarks
2. Remove duplicate `docs/EXAMPLES/`
3. Add doxygen-style API docs for all public functions

## Testing Strategy

Every fix MUST be accompanied by a test:
- `list_pop_back`: add test in `test_list.c`
- `list_get`: add test in `test_list.c`
- `list_iterator`: add test in `test_iterator.c`
- `vector_remove`: add test in `test_vector.c`
- HashMap resize: add stress test in `test_hashmap.c`

## Out of Scope for Phase 2

- New container types (Set, Deque)
- Windows IOCP support
- JSON serializer/deserializer refactoring
- API signature changes beyond error propagation
- New algorithms (binary search, find_if, etc.)
