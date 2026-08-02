# Cobalt Phase 2: Code Quality & Completeness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix critical bugs, eliminate code duplication, complete missing functionality, and improve CI/test coverage in the Cobalt C framework.

**Architecture:** Three-phase approach: Phase 2a fixes blocking bugs and stubs, Phase 2b improves error handling and CI, Phase 2c adds polish. All changes are backward-compatible with existing C API.

**Tech Stack:** C11, CMake, CTest, clang-tidy, clang-format

---

## File Structure

### Files to Create
- `src/utils/string.c` — Shared string utility (cobalt_strdup)
- `tests/benchmark/benchmark_hashmap.c` — HashMap performance benchmark
- `tests/benchmark/benchmark_vector.c` — Vector performance benchmark

### Files to Modify
- `include/cobalt/utils/string.h` — New header for cobalt_strdup
- `include/cobalt/cobalt.h` — Fix include order (sort.h before functional.h)
- `src/container/list.c` — Fix compilation error, implement pop_back/get/iterator
- `src/container/vector.c` — Implement remove_seq
- `src/container/hashmap.c` — Add error propagation
- `src/container/stack.c` — Add error propagation
- `src/container/queue.c` — Add error propagation
- `src/algorithm/functional.c` — Remove duplicate compare_func_t
- `include/cobalt/algorithm/functional.h` — Include sort.h instead of redefining compare_func_t
- `tests/unit/test_list.c` — Add tests for pop_back, get, iterator
- `tests/unit/test_vector.c` — Add test for remove
- `tests/unit/test_hashmap.c` — Add resize stress test
- `tests/CMakeLists.txt` — Add benchmark targets
- `.github/workflows/ci.yml` — Add Valgrind and ASan checks

---

## Task 1: Fix list.c Compilation Error

**Files:**
- Modify: `src/container/list.c:175-178`

- [ ] **Step 1: Read current list.c to confirm the bug**

Open `src/container/list.c` and verify that `cobalt_list_size()` contains incorrect code.

- [ ] **Step 2: Fix the compilation error**

Replace the broken `cobalt_list_size` function with the correct implementation:

```c
size_t cobalt_list_size(cobalt_list_t* list)
{
    return list ? ((cobalt_list_impl_t*)list)->size : 0;
}
```

- [ ] **Step 3: Build and verify no compilation errors**

Run: `cmake --build build --parallel`
Expected: Clean build with no errors.

- [ ] **Step 4: Run tests to verify nothing broke**

Run: `cd build && ctest --output-on-failure`
Expected: All 20 tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/container/list.c
git commit -m "fix(list): fix cobalt_list_size compilation error"
```

---

## Task 2: Implement list_pop_back()

**Files:**
- Modify: `src/container/list.c:162-166`
- Modify: `tests/unit/test_list.c`

- [ ] **Step 1: Write the failing test first**

Add this test function to `tests/unit/test_list.c` before `test_list()`:

```c
void test_list_pop_back(void)
{
    printf("Testing list_pop_back...\n");
    
    cobalt_list_t* list = cobalt_list_create();
    TEST_ASSERT(list != NULL);
    
    int val1 = 10;
    int val2 = 20;
    int val3 = 30;
    
    TEST_ASSERT(cobalt_list_push_back(list, &val1) == 0);
    TEST_ASSERT(cobalt_list_push_back(list, &val2) == 0);
    TEST_ASSERT(cobalt_list_push_back(list, &val3) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 3);
    
    void* item = cobalt_list_pop_back(list);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int*)item == 30);
    TEST_ASSERT(cobalt_list_size(list) == 2);
    
    item = cobalt_list_pop_back(list);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int*)item == 20);
    TEST_ASSERT(cobalt_list_size(list) == 1);
    
    item = cobalt_list_pop_back(list);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int*)item == 10);
    TEST_ASSERT(cobalt_list_size(list) == 0);
    
    item = cobalt_list_pop_back(list);
    TEST_ASSERT(item == NULL);
    
    cobalt_list_destroy(list);
    printf("  list_pop_back test passed\n");
}
```

Also add `test_list_pop_back();` to the `test_list()` function.

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && ctest --output-on-failure --filter test_list`
Expected: FAIL — pop_back returns NULL instead of the last element.

- [ ] **Step 3: Implement list_pop_back()**

Replace the stub in `src/container/list.c`:

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

- [ ] **Step 4: Run test to verify it passes**

Run: `cd build && ctest --output-on-failure --filter test_list`
Expected: PASS — all list tests pass including pop_back.

- [ ] **Step 5: Run full test suite**

Run: `cd build && ctest --output-on-failure`
Expected: All 20 tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/container/list.c tests/unit/test_list.c
git commit -m "feat(list): implement cobalt_list_pop_back()"
```

---

## Task 3: Implement list_get()

**Files:**
- Modify: `src/container/list.c:168-173`
- Modify: `tests/unit/test_list.c`

- [ ] **Step 1: Write the failing test first**

Add to `tests/unit/test_list.c`:

```c
void test_list_get(void)
{
    printf("Testing list_get...\n");
    
    cobalt_list_t* list = cobalt_list_create();
    TEST_ASSERT(list != NULL);
    
    int values[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++)
        TEST_ASSERT(cobalt_list_push_back(list, &values[i]) == 0);
    
    /* Test getting elements from beginning */
    void* item = cobalt_list_get(list, 0);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int*)item == 10);
    
    /* Test getting element from middle */
    item = cobalt_list_get(list, 2);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int*)item == 30);
    
    /* Test getting element from end */
    item = cobalt_list_get(list, 4);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int*)item == 50);
    
    /* Test out of bounds */
    item = cobalt_list_get(list, 5);
    TEST_ASSERT(item == NULL);
    
    item = cobalt_list_get(list, 100);
    TEST_ASSERT(item == NULL);
    
    /* Test empty list */
    cobalt_list_t* empty = cobalt_list_create();
    item = cobalt_list_get(empty, 0);
    TEST_ASSERT(item == NULL);
    cobalt_list_destroy(empty);
    
    cobalt_list_destroy(list);
    printf("  list_get test passed\n");
}
```

Add `test_list_get();` to `test_list()`.

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && ctest --output-on-failure --filter test_list`
Expected: FAIL — get returns NULL for all indices.

- [ ] **Step 3: Implement list_get()**

Replace the stub in `src/container/list.c`:

```c
void* cobalt_list_get(cobalt_list_t* list, size_t index)
{
    if (!list)
        return NULL;
    
    cobalt_list_impl_t* impl = (cobalt_list_impl_t*)list;
    if (index >= impl->size)
        return NULL;
    
    list_node_t* node;
    if (index < impl->size / 2)
    {
        node = impl->head;
        for (size_t i = 0; i < index; i++)
            node = node->next;
    }
    else
    {
        node = impl->tail;
        for (size_t i = impl->size - 1; i > index; i--)
            node = node->prev;
    }
    
    return node->data;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd build && ctest --output-on-failure --filter test_list`
Expected: PASS.

- [ ] **Step 5: Run full test suite**

Run: `cd build && ctest --output-on-failure`
Expected: All 20 tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/container/list.c tests/unit/test_list.c
git commit -m "feat(list): implement cobalt_list_get() with bidirectional traversal"
```

---

## Task 4: Implement list_iterator_seq()

**Files:**
- Modify: `src/container/list.c:61-65`
- Modify: `include/cobalt/container/list.h`
- Modify: `tests/unit/test_iterator.c`

- [ ] **Step 1: Write the failing test first**

Add to `tests/unit/test_iterator.c`:

```c
void test_iterator_with_list(void)
{
    printf("Testing iterator with list...\n");
    
    cobalt_list_t* list = cobalt_list_create();
    TEST_ASSERT(list != NULL);
    
    int values[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        TEST_ASSERT(cobalt_list_push_back(list, &values[i]) == 0);
    
    /* Get iterator from list via sequence interface */
    cobalt_sequence_t* seq = (cobalt_sequence_t*)list;
    cobalt_iterator_t* iter = cobalt_iterator_new(seq);
    TEST_ASSERT(iter != NULL);
    
    int count = 0;
    while (cobalt_iterator_has_next(iter))
    {
        cobalt_iterator_next(iter);
        count++;
    }
    
    TEST_ASSERT(count == 5);
    
    cobalt_iterator_destroy(iter);
    cobalt_list_destroy(list);
    printf("  List iterator test passed\n");
}
```

Add `test_iterator_with_list();` to `test_iterator()`.

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && ctest --output-on-failure --filter test_iterator`
Expected: FAIL — iterator returns 0 elements for list.

- [ ] **Step 3: Implement list_iterator_seq()**

Replace the stub in `src/container/list.c`:

```c
static cobalt_iterator_t* list_iterator_seq(cobalt_sequence_t* self)
{
    cobalt_list_impl_t* list = (cobalt_list_impl_t*)self;
    return cobalt_iterator_new(self);
}
```

The existing `cobalt_iterator_new()` already uses `seq->size()` to determine total count, and since list's `size` callback is implemented, this will work. However, `cobalt_iterator_next()` currently returns NULL (it's a stub). For list iteration to work, we need to track position. Let me check the iterator implementation...

Actually, the iterator uses `seq->size()` for total count but `cobalt_iterator_next()` returns NULL. We need to enhance the iterator to support list traversal. Add a list-specific iterator creation:

In `include/cobalt/container/list.h`, add:
```c
/* List-specific iterator creation */
cobalt_iterator_t* cobalt_list_iterator_create(cobalt_list_t* list);
```

In `src/container/list.c`, implement:
```c
/* List iterator implementation */
typedef struct
{
    cobalt_iterator_t base;
    cobalt_list_impl_t* list;
    list_node_t* current;
} list_iterator_impl_t;

static int list_iterator_has_next(cobalt_iterator_t* iter)
{
    list_iterator_impl_t* impl = (list_iterator_impl_t*)iter;
    return impl->current != NULL;
}

static void* list_iterator_next(cobalt_iterator_t* iter)
{
    list_iterator_impl_t* impl = (list_iterator_impl_t*)iter;
    if (!impl->current)
        return NULL;
    
    void* data = impl->current->data;
    impl->current = impl->current->next;
    return data;
}

cobalt_iterator_t* cobalt_list_iterator_create(cobalt_list_t* list)
{
    if (!list)
        return NULL;
    
    cobalt_list_impl_t* impl = (cobalt_list_impl_t*)list;
    list_iterator_impl_t* iter = malloc(sizeof(list_iterator_impl_t));
    if (!iter)
        return NULL;
    
    iter->list = impl;
    iter->current = impl->head;
    iter->base.has_next = list_iterator_has_next;
    iter->base.next = list_iterator_next;
    iter->base.destroy = cobalt_iterator_destroy;
    
    return (cobalt_iterator_t*)iter;
}
```

Update `list_iterator_seq()` to use this:
```c
static cobalt_iterator_t* list_iterator_seq(cobalt_sequence_t* self)
{
    cobalt_list_impl_t* list = (cobalt_list_impl_t*)self;
    return cobalt_list_iterator_create((cobalt_list_t*)list);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd build && ctest --output-on-failure --filter test_iterator`
Expected: PASS — list iterator traverses all 5 elements.

- [ ] **Step 5: Run full test suite**

Run: `cd build && ctest --output-on-failure`
Expected: All 20 tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/container/list.c include/cobalt/container/list.h tests/unit/test_iterator.c
git commit -m "feat(list): implement list-specific iterator for Sequence interface"
```

---

## Task 5: Implement vector_remove_seq()

**Files:**
- Modify: `src/container/vector.c:41-45`
- Modify: `tests/unit/test_vector.c`

- [ ] **Step 1: Write the failing test first**

Add to `tests/unit/test_vector.c`:

```c
void test_vector_remove(void)
{
    printf("Testing vector_remove...\n");
    
    cobalt_vector_t* vec = cobalt_vector_create(4);
    TEST_ASSERT(vec != NULL);
    
    int values[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++)
        TEST_ASSERT(cobalt_vector_push(vec, &values[i]) == 0);
    
    TEST_ASSERT(cobalt_vector_size(vec) == 5);
    
    /* Remove middle element */
    cobalt_sequence_t* seq = (cobalt_sequence_t*)vec;
    seq->remove(seq, &values[2]);  /* Remove 30 */
    
    TEST_ASSERT(cobalt_vector_size(vec) == 4);
    TEST_ASSERT(*(int*)cobalt_vector_get(vec, 0) == 10);
    TEST_ASSERT(*(int*)cobalt_vector_get(vec, 1) == 20);
    TEST_ASSERT(*(int*)cobalt_vector_get(vec, 2) == 40);
    TEST_ASSERT(*(int*)cobalt_vector_get(vec, 3) == 50);
    
    /* Remove first element */
    seq->remove(seq, &values[0]);
    TEST_ASSERT(cobalt_vector_size(vec) == 3);
    TEST_ASSERT(*(int*)cobalt_vector_get(vec, 0) == 20);
    
    /* Remove last element */
    seq->remove(seq, &values[4]);
    TEST_ASSERT(cobalt_vector_size(vec) == 2);
    
    cobalt_vector_destroy(vec);
    printf("  vector_remove test passed\n");
}
```

Add `test_vector_remove();` to `test_vector()`.

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && ctest --output-on-failure --filter test_vector`
Expected: FAIL — size doesn't change after remove.

- [ ] **Step 3: Implement vector_remove_seq()**

Replace the stub in `src/container/vector.c`:

```c
static void vector_remove_seq(cobalt_sequence_t* self, void* item)
{
    cobalt_vector_impl_t* vec = (cobalt_vector_impl_t*)self;
    
    for (size_t i = 0; i < vec->size; i++)
    {
        if (vec->items[i] == item)
        {
            memmove(vec->items + i, vec->items + i + 1,
                    (vec->size - i - 1) * sizeof(void*));
            vec->size--;
            return;
        }
    }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd build && ctest --output-on-failure --filter test_vector`
Expected: PASS.

- [ ] **Step 5: Run full test suite**

Run: `cd build && ctest --output-on-failure`
Expected: All 20 tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/container/vector.c tests/unit/test_vector.c
git commit -m "feat(vector): implement sequence remove for contiguous memory"
```

---

## Task 6: Unify my_strdup into Shared Utility

**Files:**
- Create: `include/cobalt/utils/string.h`
- Create: `src/utils/string.c`
- Modify: `src/container/hashmap.c`
- Modify: `src/container/treemap.c`
- Modify: `src/module/json.c`
- Modify: `src/core/class.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the shared header**

Create `include/cobalt/utils/string.h`:

```c
#ifndef COBALT_STRING_UTIL_H
#define COBALT_STRING_UTIL_H

/**
 * @file string.h
 * @brief Portable string utilities
 */

#include <stddef.h>

/**
 * @brief Duplicate a string (C11 portable strdup)
 * @param s Source string (may be NULL)
 * @return Duplicate string or NULL on failure
 */
char* cobalt_strdup(const char* s);

#endif /* COBALT_STRING_UTIL_H */
```

- [ ] **Step 2: Create the implementation**

Create `src/utils/string.c`:

```c
#include "cobalt/utils/string.h"
#include <stdlib.h>
#include <string.h>

char* cobalt_strdup(const char* s)
{
    if (!s)
        return NULL;
    
    size_t len = strlen(s) + 1;
    char* dup = malloc(len);
    if (dup)
        memcpy(dup, s, len);
    
    return dup;
}
```

- [ ] **Step 3: Update CMakeLists.txt to include new source**

Add `src/utils/string.c` to the SOURCES list in `CMakeLists.txt`.

- [ ] **Step 4: Update hashmap.c**

Remove the local `my_strdup` function and replace with:
```c
#include "cobalt/utils/string.h"
```
Then replace `my_strdup(key)` with `cobalt_strdup(key)`.

- [ ] **Step 5: Update treemap.c**

Same as hashmap.c — remove local `my_strdup`, include the shared header, use `cobalt_strdup()`.

- [ ] **Step 6: Update json.c**

Remove the local `my_strdup` function, include the shared header, replace all `my_strdup` calls with `cobalt_strdup`.

- [ ] **Step 7: Update class.c**

Remove the local `my_strdup` function, include the shared header, replace `my_strdup(name)` with `cobalt_strdup(name)`.

- [ ] **Step 8: Build and verify**

Run: `cmake --build build --parallel`
Expected: Clean build.

- [ ] **Step 9: Run tests**

Run: `cd build && ctest --output-on-failure`
Expected: All 20 tests pass.

- [ ] **Step 10: Commit**

```bash
git add include/cobalt/utils/string.h src/utils/string.c \
        src/container/hashmap.c src/container/treemap.c \
        src/module/json.c src/core/class.c CMakeLists.txt
git commit -m "refactor: unify my_strdup into shared cobalt_strdup utility"
```

---

## Task 7: Fix Header Include Order and Duplicate Typedef

**Files:**
- Modify: `include/cobalt/algorithm/functional.h`
- Modify: `include/cobalt/cobalt.h`

- [ ] **Step 1: Fix functional.h to include sort.h**

In `include/cobalt/algorithm/functional.h`, replace:
```c
/* Comparison function type (shared with sort.h) */
typedef int (*compare_func_t)(const void* a, const void* b);
```

With:
```c
#include "cobalt/algorithm/sort.h"
```

- [ ] **Step 2: Fix cobalt.h include order**

In `include/cobalt/cobalt.h`, ensure `sort.h` is included before `functional.h`:

```c
#include <cobalt/algorithm/sort.h>
#include <cobalt/algorithm/functional.h>
```

- [ ] **Step 3: Build and verify**

Run: `cmake --build build --parallel`
Expected: Clean build.

- [ ] **Step 4: Run tests**

Run: `cd build && ctest --output-on-failure`
Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add include/cobalt/algorithm/functional.h include/cobalt/cobalt.h
git commit -m "fix: remove duplicate compare_func_t typedef, fix include order"
```

---

## Task 8: Add Error Propagation to Containers

**Files:**
- Modify: `src/container/vector.c`
- Modify: `src/container/list.c`
- Modify: `src/container/stack.c`
- Modify: `src/container/queue.c`
- Modify: `tests/unit/test_vector.c`
- Modify: `tests/unit/test_list.c`
- Modify: `tests/unit/test_stack.c`
- Modify: `tests/unit/test_queue.c`

- [ ] **Step 1: Add error propagation to vector**

In `src/container/vector.c`, add `#include "cobalt/runtime/error.h"` and update failure paths:

```c
// In cobalt_vector_push, after malloc failure:
cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
return -1;

// In cobalt_vector_set, for null vec:
cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
return -1;

// In cobalt_vector_set, for out of bounds:
cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
return -1;
```

- [ ] **Step 2: Add error propagation to list**

Same pattern in `src/container/list.c` for all failure paths in push_front, push_back.

- [ ] **Step 3: Add error propagation to stack**

Same pattern in `src/container/stack.c` for push failure.

- [ ] **Step 4: Add error propagation to queue**

Same pattern in `src/container/queue.c` for enqueue failure.

- [ ] **Step 5: Add tests for error propagation**

Add to each test file:

```c
void test_vector_error_propagation(void)
{
    printf("Testing vector error propagation...\n");
    
    cobalt_error_t err = COBALT_SUCCESS;
    
    /* NULL should set error */
    int ret = cobalt_vector_push(NULL, NULL, &err);
    TEST_ASSERT(ret == -1);
    TEST_ASSERT(err == COBALT_ERROR_INVALID_ARGUMENT);
    
    printf("  Error propagation test passed\n");
}
```

- [ ] **Step 6: Build and run tests**

Run: `cmake --build build --parallel && ctest --output-on-failure`
Expected: All tests pass.

- [ ] **Step 7: Commit**

```bash
git add src/container/vector.c src/container/list.c \
        src/container/stack.c src/container/queue.c \
        tests/unit/test_vector.c tests/unit/test_list.c \
        tests/unit/test_stack.c tests/unit/test_queue.c
git commit -m "feat(error): propagate errors through all container operations"
```

---

## Task 9: Add HashMap Resize Stress Test

**Files:**
- Modify: `tests/unit/test_hashmap.c`

- [ ] **Step 1: Add resize stress test**

Add to `tests/unit/test_hashmap.c`:

```c
void test_hashmap_resize_stress(void)
{
    printf("Testing hashmap resize stress...\n");
    
    cobalt_hashmap_t* map = cobalt_hashmap_create(4);
    TEST_ASSERT(map != NULL);
    
    size_t initial_capacity = cobalt_hashmap_capacity(map);
    TEST_ASSERT(initial_capacity == 4);
    
    /* Insert enough to trigger multiple resizes */
    int values[200];
    char keys[200][32];
    
    for (int i = 0; i < 200; i++)
    {
        snprintf(keys[i], 32, "stress_key_%d", i);
        values[i] = i * 100;
        TEST_ASSERT(cobalt_hashmap_put(map, keys[i], &values[i]) == 0);
    }
    
    TEST_ASSERT(cobalt_hashmap_size(map) == 200);
    TEST_ASSERT(cobalt_hashmap_capacity(map) > initial_capacity);
    
    /* Verify all values still accessible after resizes */
    for (int i = 0; i < 200; i++)
    {
        void* val = cobalt_hashmap_get(map, keys[i]);
        TEST_ASSERT(val != NULL);
        TEST_ASSERT(*(int*)val == i * 100);
    }
    
    cobalt_hashmap_destroy(map);
    printf("  Resize stress test passed\n");
}
```

Add `test_hashmap_resize_stress();` to `test_hashmap()`.

- [ ] **Step 2: Run tests**

Run: `cd build && ctest --output-on-failure --filter test_hashmap`
Expected: PASS.

- [ ] **Step 3: Run full suite**

Run: `cd build && ctest --output-on-failure`
Expected: All tests pass.

- [ ] **Step 4: Commit**

```bash
git add tests/unit/test_hashmap.c
git commit -m "test(hashmap): add resize stress test for load factor verification"
```

---

## Task 10: Add CI with Valgrind and ASan

**Files:**
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: Add Valgrind step to build job**

Add after the "Run tests" step in `.github/workflows/ci.yml`:

```yaml
- name: Install Valgrind
  run: sudo apt-get install -y valgrind

- name: Run Valgrind memory check
  working-directory: ${{github.workspace}}/build
  run: valgrind --leak-check=full --error-exitcode=1 ./tests/cobalt_test
```

- [ ] **Step 2: Add ASan build job**

Add a new job after the existing build job:

```yaml
  asan:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4
    
    - name: Configure with ASan
      run: cmake -B ${{github.workspace}}/build-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address -g" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
    
    - name: Build
      run: cmake --build ${{github.workspace}}/build-asan --parallel
    
    - name: Run tests with ASan
      working-directory: ${{github.workspace}}/build-asan
      run: ctest --output-on-failure
```

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: add Valgrind memory checks and ASan build"
```

---

## Task 11: Add Performance Benchmarks

**Files:**
- Create: `tests/benchmark/benchmark_hashmap.c`
- Create: `tests/benchmark/benchmark_vector.c`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create hashmap benchmark**

Create `tests/benchmark/benchmark_hashmap.c`:

```c
#include "cobalt/container/hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main(void)
{
    printf("HashMap Benchmark\n");
    printf("=================\n");
    
    /* Test 1: Insert 10K strings */
    cobalt_hashmap_t* map = cobalt_hashmap_create(1024);
    char key[64];
    int value = 42;
    
    double start = current_time_ms();
    for (int i = 0; i < 10000; i++)
    {
        snprintf(key, 64, "key_%d", i);
        cobalt_hashmap_put(map, key, &value);
    }
    double insert_time = current_time_ms() - start;
    printf("Insert 10K strings: %.2f ms\n", insert_time);
    
    /* Test 2: Get 10K strings */
    start = current_time_ms();
    for (int i = 0; i < 10000; i++)
    {
        snprintf(key, 64, "key_%d", i);
        cobalt_hashmap_get(map, key);
    }
    double get_time = current_time_ms() - start;
    printf("Get 10K strings: %.2f ms\n", get_time);
    
    cobalt_hashmap_destroy(map);
    
    printf("Benchmark completed\n");
    return 0;
}
```

- [ ] **Step 2: Create vector benchmark**

Create `tests/benchmark/benchmark_vector.c`:

```c
#include "cobalt/container/vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main(void)
{
    printf("Vector Benchmark\n");
    printf("================\n");
    
    /* Test 1: Push 1M elements */
    cobalt_vector_t* vec = cobalt_vector_create(1024);
    int value = 42;
    
    double start = current_time_ms();
    for (int i = 0; i < 1000000; i++)
    {
        cobalt_vector_push(vec, &value);
    }
    double push_time = current_time_ms() - start;
    printf("Push 1M elements: %.2f ms\n", push_time);
    
    /* Test 2: Get 1M elements */
    start = current_time_ms();
    for (int i = 0; i < 1000000; i++)
    {
        cobalt_vector_get(vec, i);
    }
    double get_time = current_time_ms() - start;
    printf("Get 1M elements: %.2f ms\n", get_time);
    
    cobalt_vector_destroy(vec);
    
    printf("Benchmark completed\n");
    return 0;
}
```

- [ ] **Step 3: Update tests/CMakeLists.txt**

Add:

```cmake
# Benchmarks
add_executable(benchmark_hashmap tests/benchmark/benchmark_hashmap.c)
target_link_libraries(benchmark_hashmap cobalt)

add_executable(benchmark_vector tests/benchmark/benchmark_vector.c)
target_link_libraries(benchmark_vector cobalt)
```

- [ ] **Step 4: Build and run benchmarks**

Run: `cmake --build build --parallel && ./build/tests/benchmark_hashmap && ./build/tests/benchmark_vector`
Expected: Benchmarks run and print timing results.

- [ ] **Step 5: Commit**

```bash
git add tests/benchmark/ tests/CMakeLists.txt
git commit -m "feat(benchmark): add hashmap and vector micro-benchmarks"
```

---

## Verification

After completing all tasks:

1. Run full test suite: `cd build && ctest --output-on-failure`
   - Expected: 20/20 tests pass (or more with new tests)

2. Run benchmarks: `./build/tests/benchmark_hashmap && ./build/tests/benchmark_vector`

3. Run clang-tidy: `cmake --build build --target tidy`

4. Run clang-format check: `cmake --build build --target format`

5. Verify all examples build and run:
   ```bash
   for exe in examples_*; do
       echo "Running $exe..."
       ./build/$exe
   done
   ```
