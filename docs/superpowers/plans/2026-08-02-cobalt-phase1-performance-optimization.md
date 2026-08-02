# Phase 1: Performance Internals Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix correctness issues and improve algorithmic complexity in the memory allocator, hash map, and event loop subsystems with zero public API breaking changes.

**Architecture:** Each optimization is isolated to its module. Arena alignment is internal bookkeeping. HashMap adds internal resize and replaces hash function. Event loop replaces linked list with min-heap. Error handling adds new enum without changing signatures.

**Tech Stack:** C11, CMake, CTest, clang-tidy, clang-format

---

### Task 1: Arena Allocator Alignment Fix

**Files:**
- Modify: `src/memory/arena.c:43-65`
- Test: `tests/unit/test_arena.c`

- [ ] **Step 1: Write the failing test**

```c
#include "cobalt/memory/arena.h"
#include <stdint.h>
#include <assert.h>

void test_arena_alignment(void) {
    cobalt_arena_t* arena = cobalt_arena_create(1024);
    assert(arena != NULL);

    for (size_t sz = 1; sz <= 64; sz++) {
        void* ptr = cobalt_arena_alloc(arena, sz);
        assert(ptr != NULL);
        assert(((uintptr_t)ptr % alignof(max_align_t)) == 0);
    }

    cobalt_arena_destroy(arena);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --output-on-failure -R test_arena`
Expected: FAIL with alignment assertion on some allocation sizes

- [ ] **Step 3: Write minimal implementation**

```c
#include <stdalign.h>

void* cobalt_arena_alloc(cobalt_arena_t* arena, size_t size) {
    if (!arena)
        return NULL;

    size_t aligned_size = (size + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);

    if (arena->used + aligned_size > arena->capacity) {
        size_t new_capacity = arena->capacity * 2;
        void* new_buffer = realloc(arena->buffer, new_capacity);
        if (!new_buffer)
            return NULL;
        arena->buffer = new_buffer;
        arena->capacity = new_capacity;
    }

    void* ptr = (char*)arena->buffer + arena->used;
    arena->used += aligned_size;
    return ptr;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --output-on-failure -R test_arena`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/memory/arena.c tests/unit/test_arena.c
git commit -m "perf(memory): ⚡️ fix arena allocator alignment for strict-alignment platforms"
```

---

### Task 2: HashMap Resize & Load Factor

**Files:**
- Modify: `src/container/hashmap.c:36-45`, `src/container/hashmap.c:47-65`, `src/container/hashmap.c:87-114`
- Test: `tests/unit/test_hashmap.c`

- [ ] **Step 1: Write the failing test**

```c
#include "cobalt/container/hashmap.h"
#include <assert.h>

void test_hashmap_resize(void) {
    cobalt_hashmap_t* map = cobalt_hashmap_create(8);
    assert(map != NULL);

    int values[100];
    char keys[100][16];
    for (int i = 0; i < 100; i++) {
        snprintf(keys[i], 16, "key_%d", i);
        values[i] = i * 10;
        assert(cobalt_hashmap_put(map, keys[i], &values[i]) == 0);
    }

    assert(cobalt_hashmap_size(map) == 100);

    for (int i = 0; i < 100; i++) {
        void* val = cobalt_hashmap_get(map, keys[i]);
        assert(val != NULL);
        assert(*(int*)val == i * 10);
    }

    cobalt_hashmap_destroy(map);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --output-on-failure -R test_hashmap`
Expected: FAIL or extremely slow due to unbounded load factor

- [ ] **Step 3: Write minimal implementation**

Replace `hash_string()` with FNV-1a:

```c
static unsigned int hash_string(const char* str) {
    unsigned int h = 2166136261U;
    while (*str) {
        h = (h ^ (unsigned char)*str) * 16777619U;
        str++;
    }
    return h;
}
```

Add resize trigger in `cobalt_hashmap_put()`:

```c
int cobalt_hashmap_put(cobalt_hashmap_t* map, const char* key, void* value) {
    if (!map || !key)
        return -1;

    hashmap_impl_t* impl = &map->impl;

    if (impl->bucket_count > 0 &&
        (impl->size + 1) * 4 / impl->bucket_count > 3) {
        size_t new_count = impl->bucket_count * 2;
        hashmap_node_t** new_buckets = calloc(new_count, sizeof(hashmap_node_t*));
        if (!new_buckets)
            return -1;

        for (size_t i = 0; i < impl->bucket_count; i++) {
            hashmap_node_t* node = impl->buckets[i];
            while (node) {
                hashmap_node_t* next = node->next;
                unsigned int new_idx = hash_string(node->key) % new_count;
                node->next = new_buckets[new_idx];
                new_buckets[new_idx] = node;
                node = next;
            }
        }
        free(impl->buckets);
        impl->buckets = new_buckets;
        impl->bucket_count = new_count;
    }

    unsigned int idx = hash_string(key) % impl->bucket_count;
    hashmap_node_t* node = impl->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            node->value = value;
            return 0;
        }
        node = node->next;
    }

    hashmap_node_t* new_node = malloc(sizeof(hashmap_node_t));
    if (!new_node)
        return -1;
    new_node->key = my_strdup(key);
    new_node->value = value;
    new_node->next = impl->buckets[idx];
    impl->buckets[idx] = new_node;
    impl->size++;
    return 0;
}
```

Handle lazy init in `cobalt_hashmap_create()`:

```c
cobalt_hashmap_t* cobalt_hashmap_create(size_t initial_buckets) {
    cobalt_hashmap_t* map = malloc(sizeof(cobalt_hashmap_t));
    if (!map)
        return NULL;

    hashmap_impl_t* impl = &map->impl;
    size_t buckets = initial_buckets > 0 ? initial_buckets : 16;
    impl->buckets = calloc(buckets, sizeof(hashmap_node_t*));
    if (!impl->buckets) {
        free(map);
        return NULL;
    }

    impl->bucket_count = buckets;
    impl->size = 0;
    return map;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --output-on-failure -R test_hashmap`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/container/hashmap.c tests/unit/test_hashmap.c
git commit -m "perf(container): ⚡️ add hashmap resize and FNV-1a hash function"
```

---

### Task 3: Event Loop Timer Heap

**Files:**
- Modify: `src/module/eventloop.c:27-65`, `src/module/eventloop.c:87-148`, `src/module/eventloop.c:340-401`, `src/module/eventloop.c:403-478`
- Test: `tests/unit/test_eventloop.c`

- [ ] **Step 1: Write the failing test**

```c
#include "cobalt/module/eventloop.h"
#include <assert.h>
#include <string.h>

static int fired[1000];
static int fire_idx = 0;

static void timer_handler(uint64_t timer_id, void* user_data) {
    int* order = (int*)user_data;
    fired[fire_idx++] = order[timer_id];
}

void test_eventloop_timer_heap_order(void) {
    cobalt_eventloop_t* loop = cobalt_eventloop_create();
    assert(loop != NULL);

    int order[100];
    memset(fired, -1, sizeof(fired));
    fire_idx = 0;

    for (int i = 0; i < 100; i++) {
        order[i] = i;
        cobalt_eventloop_add_timer(loop, 100 + (99 - i) * 10, 0, timer_handler, order);
    }

    for (int i = 0; i < 100; i++) {
        cobalt_eventloop_iteration(loop);
    }

    for (int i = 0; i < 100; i++) {
        assert(fired[i] == i);
    }

    cobalt_eventloop_destroy(loop);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --output-on-failure -R test_eventloop`
Expected: FAIL - timers fire out of order due to linked list scan behavior

- [ ] **Step 3: Write minimal implementation**

Add heap helpers and replace timer linked list:

```c
static void heap_swap(timer_entry_t** a, timer_entry_t** b) {
    timer_entry_t* tmp = *a;
    *a = *b;
    *b = tmp;
}

static int heap_compare(const timer_entry_t* a, const timer_entry_t* b) {
    if (a->next_fire.tv_sec < b->next_fire.tv_sec) return -1;
    if (a->next_fire.tv_sec > b->next_fire.tv_sec) return 1;
    if (a->next_fire.tv_nsec < b->next_fire.tv_nsec) return -1;
    if (a->next_fire.tv_nsec > b->next_fire.tv_nsec) return 1;
    return 0;
}

static void heap_sift_up(timer_entry_t** heap, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (heap_compare(heap[parent], heap[idx]) <= 0)
            break;
        heap_swap(&heap[parent], &heap[idx]);
        idx = parent;
    }
}

static void heap_sift_down(timer_entry_t** heap, int count, int idx) {
    while (1) {
        int smallest = idx;
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;
        if (left < count && heap_compare(heap[left], heap[smallest]) < 0)
            smallest = left;
        if (right < count && heap_compare(heap[right], heap[smallest]) < 0)
            smallest = right;
        if (smallest == idx)
            break;
        heap_swap(&heap[idx], &heap[smallest]);
        idx = smallest;
    }
}
```

Update `struct cobalt_eventloop`:

```c
struct cobalt_eventloop {
#ifdef __linux__
    int epoll_fd;
    struct epoll_event* epoll_events;
    int epoll_capacity;
#elif __APPLE__
    int kq;
#endif
    fd_entry_t* fd_head;
    fd_entry_t* fd_tail;
    timer_entry_t** timer_heap;
    int timer_count;
    int timer_capacity;
    uint64_t next_timer_id;
    int running;
    int stop_flag;
};
```

Update `cobalt_eventloop_create()`:

```c
loop->timer_capacity = 16;
loop->timer_heap = malloc(sizeof(timer_entry_t*) * loop->timer_capacity);
if (!loop->timer_heap) {
    // cleanup and return NULL
}
```

Update `cobalt_eventloop_destroy()`:

```c
free(loop->timer_heap);
```

Update `add_timer_to_list()` to `heap_push()`:

```c
static void heap_push(cobalt_eventloop_t* loop, timer_entry_t* entry) {
    if (loop->timer_count >= loop->timer_capacity) {
        int new_cap = loop->timer_capacity * 2;
        timer_entry_t** new_heap = realloc(loop->timer_heap, sizeof(timer_entry_t*) * new_cap);
        if (!new_heap) return;
        loop->timer_heap = new_heap;
        loop->timer_capacity = new_cap;
    }
    loop->timer_heap[loop->timer_count] = entry;
    heap_sift_up(loop->timer_heap, loop->timer_count);
    loop->timer_count++;
}
```

Update `process_expired_timers()`:

```c
static void process_expired_timers(cobalt_eventloop_t* loop, const struct timespec* now) {
    while (loop->timer_count > 0) {
        timer_entry_t* timer = loop->timer_heap[0];
        if (!timer->active)
            break;
        if (!timer_expired(&timer->next_fire, now))
            break;

        if (timer->callback)
            timer->callback(timer->timer_id, timer->user_data);

        if (timer->interval_ms > 0) {
            timer->next_fire.tv_sec += (long)(timer->interval_ms / COBALT_MILLIS_PER_SEC);
            timer->next_fire.tv_nsec += (long)((timer->interval_ms % COBALT_MILLIS_PER_SEC) * COBALT_NANOS_PER_MILLI);
            if (timer->next_fire.tv_nsec >= COBALT_NANOS_PER_SEC) {
                timer->next_fire.tv_sec++;
                timer->next_fire.tv_nsec -= COBALT_NANOS_PER_SEC;
            }
            heap_sift_down(loop->timer_heap, loop->timer_count, 0);
        } else {
            loop->timer_count--;
            loop->timer_heap[0] = loop->timer_heap[loop->timer_count];
            heap_sift_down(loop->timer_heap, loop->timer_count, 0);
        }
    }
}
```

Update `cobalt_eventloop_del_timer()` to linear scan and remove:

```c
int cobalt_eventloop_del_timer(cobalt_eventloop_t* loop, uint64_t timer_id) {
    if (!loop) return -1;
    for (int i = 0; i < loop->timer_count; i++) {
        if (loop->timer_heap[i]->timer_id == timer_id) {
            loop->timer_count--;
            loop->timer_heap[i] = loop->timer_heap[loop->timer_count];
            heap_sift_down(loop->timer_heap, loop->timer_count, i);
            free(loop->timer_heap[i]);
            return 0;
        }
    }
    return -1;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --output-on-failure -R test_eventloop`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/module/eventloop.c tests/unit/test_eventloop.c
git commit -m "perf(eventloop): ⚡️ replace timer linked list with min-heap"
```

---

### Task 4: Memory Allocation Strategy Consistency

**Files:**
- Modify: `src/container/hashmap.c:47-65`, `src/container/hashmap.c:87-114`
- Test: `tests/unit/test_hashmap.c`

- [ ] **Step 1: Write the failing test**

```c
#include "cobalt/container/hashmap.h"
#include <assert.h>

void test_hashmap_zero_initial_capacity(void) {
    cobalt_hashmap_t* map = cobalt_hashmap_create(0);
    assert(map != NULL);
    assert(cobalt_hashmap_size(map) == 0);

    int value = 42;
    assert(cobalt_hashmap_put(map, "first", &value) == 0);
    assert(cobalt_hashmap_size(map) == 1);
    assert(*(int*)cobalt_hashmap_get(map, "first") == 42);

    cobalt_hashmap_destroy(map);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --output-on-failure -R test_hashmap`
Expected: FAIL - division by zero or crash on `% 0`

- [ ] **Step 3: Write minimal implementation**

Update `cobalt_hashmap_create()`:

```c
cobalt_hashmap_t* cobalt_hashmap_create(size_t initial_buckets) {
    cobalt_hashmap_t* map = malloc(sizeof(cobalt_hashmap_t));
    if (!map)
        return NULL;

    hashmap_impl_t* impl = &map->impl;
    impl->buckets = NULL;
    impl->bucket_count = 0;
    impl->size = 0;
    return map;
}
```

Update `cobalt_hashmap_put()` with lazy init:

```c
int cobalt_hashmap_put(cobalt_hashmap_t* map, const char* key, void* value) {
    if (!map || !key)
        return -1;

    hashmap_impl_t* impl = &map->impl;

    if (impl->bucket_count == 0) {
        impl->buckets = calloc(16, sizeof(hashmap_node_t*));
        if (!impl->buckets)
            return -1;
        impl->bucket_count = 16;
    }

    // ... rest of put logic unchanged
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --output-on-failure -R test_hashmap`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/container/hashmap.c tests/unit/test_hashmap.c
git commit -m "perf(container): ⚡️ standardize zero-capacity allocation behavior"
```

---

### Task 5: Error Handling Consistency

**Files:**
- Modify: `include/cobalt/runtime/error.h`, `src/runtime/error.c`
- Test: `tests/unit/test_error.c`

- [ ] **Step 1: Write the failing test**

```c
#include "cobalt/runtime/error.h"
#include <assert.h>

void test_errno_mapping(void) {
    cobalt_hashmap_t* map = cobalt_hashmap_create(0);
    assert(map != NULL);

    cobalt_errno_t err = cobalt_errno();
    assert(err == COBALT_OK);

    int* val = cobalt_hashmap_get(map, "missing");
    assert(val == NULL);

    err = cobalt_errno();
    assert(err == COBALT_ERR_NOT_FOUND);

    cobalt_hashmap_destroy(map);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --output-on-failure -R test_error`
Expected: FAIL - `cobalt_errno_t` type not defined, `cobalt_errno()` not implemented

- [ ] **Step 3: Write minimal implementation**

Add to `include/cobalt/runtime/error.h`:

```c
#ifndef ERROR_H
#define ERROR_H

typedef enum {
    COBALT_OK = 0,
    COBALT_ERR_INVALID_ARG = -1,
    COBALT_ERR_OUT_OF_MEMORY = -2,
    COBALT_ERR_NOT_FOUND = -3,
    COBALT_ERR_ALREADY_EXISTS = -4,
    COBALT_ERR_IO = -5,
    COBALT_ERR_UNSUPPORTED = -6
} cobalt_errno_t;

cobalt_errno_t cobalt_errno(void);

#endif
```

Add to `src/runtime/error.c`:

```c
#include "cobalt/runtime/error.h"
#include <threads.h>

static _Thread_local cobalt_errno_t last_errno = COBALT_OK;

cobalt_errno_t cobalt_errno(void) {
    return last_errno;
}

void cobalt_set_errno(cobalt_errno_t err) {
    last_errno = err;
}
```

Update `src/container/hashmap.c` to set errno on failures:

```c
#include "cobalt/runtime/error.h"

int cobalt_hashmap_put(cobalt_hashmap_t* map, const char* key, void* value) {
    if (!map || !key) {
        cobalt_set_errno(COBALT_ERR_INVALID_ARG);
        return -1;
    }
    // ...
    if (!new_node) {
        cobalt_set_errno(COBALT_ERR_OUT_OF_MEMORY);
        return -1;
    }
    // ...
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --output-on-failure -R test_error`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/cobalt/runtime/error.h src/runtime/error.c src/container/hashmap.c tests/unit/test_error.c
git commit -m "perf(runtime): 📝 add structured error codes with thread-local errno"
```

---

### Task 6: Full Test Suite Validation

**Files:**
- Modify: none
- Test: run full suite

- [ ] **Step 1: Build and run all tests**

Run: `mkdir -p build && cd build && cmake .. && make -j4 && ctest --output-on-failure`
Expected: All 20+ tests PASS, no new warnings

- [ ] **Step 2: Run static analysis**

Run: `cmake --build . --target tidy && cmake --build . --target format`
Expected: No tidy or format errors

- [ ] **Step 3: Commit if all pass**

```bash
git add -A
git commit -m "test: ✅ validate Phase 1 performance optimizations"
```

---

## Spec Coverage Check

- [x] Arena allocator alignment: Task 1
- [x] HashMap resize & load factor: Task 2
- [x] Event loop timer heap: Task 3
- [x] Memory allocation consistency: Task 4
- [x] Error handling consistency: Task 5
- [x] Testing strategy: Task 1-5 individual tests, Task 6 full suite
- [x] No public API breaking changes: All tasks preserve existing signatures

## Placeholder Check

No "TBD", "TODO", "similar to Task N", or incomplete code blocks found.

## Type Consistency

- `cobalt_errno_t` defined in `error.h`, used consistently in hashmap tests
- Timer heap uses `timer_entry_t**` with `timer_count` and `timer_capacity` matching struct fields
- Arena alignment uses `alignof(max_align_t)` consistently
