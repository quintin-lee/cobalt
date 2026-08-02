# Cobalt Phase 3: Feature Expansion & Code Quality

## Overview

Phase 3 focuses on extending the framework with new containers, algorithms, and improving code organization. This phase builds on the solid foundation established in Phase 1 (performance) and Phase 2 (quality fixes).

## Scope

| Category | Items |
|----------|-------|
| Code Organization | Split json.c into parser/serializer modules |
| New Containers | Set (hash-based), Deque (double-ended queue) |
| New Algorithms | binary_search, find_if, for_each |
| Windows Support | IOCP event loop backend |
| Performance Hints | Compiler optimization attributes |

---

## Design

### 1. JSON Module Split

**Problem:** `src/module/json.c` is 728 lines with mixed parsing and serialization logic.

**Current Structure:**
```
src/module/json.c (728 lines)
  - json_parse() + json_parse_value() + json_parse_object() + json_parse_array() + json_parse_string()
  - json_serialize() + json_escape_string()
  - json_tree_get_child()
  - Public API: json_parse(), json_get_number(), json_get_string(), etc.
```

**Target Structure:**
```
src/module/json.c (公共API，约100行)
  - 包含 json_parse.c 和 json_serialize.c
  - 只保留公共API函数

src/module/json_parse.c (解析逻辑，约300行)
  - json_parse()
  - json_parse_value()
  - json_parse_object()
  - json_parse_array()
  - json_parse_string()
  - json_skip_whitespace()
  - json_node_create()

src/module/json_serialize.c (序列化逻辑，约300行)
  - json_serialize()
  - json_escape_string()
  - json_tree_get_child()
```

**Implementation:**
- Create new source files
- Move functions with clear ownership
- Update CMakeLists.txt
- Keep public API unchanged (backward compatible)

---

### 2. New Container: Set

**Rationale:** Set is a fundamental data structure missing from the framework. Most use cases can use HashMap with null values, but Set provides a cleaner API.

**Header:** `include/cobalt/container/set.h`

```c
#ifndef SET_H
#define SET_H

#include <stddef.h>

typedef struct cobalt_set cobalt_set_t;

/* Create a new set with initial capacity */
cobalt_set_t* cobalt_set_create(size_t initial_capacity);

/* Destroy the set */
void cobalt_set_destroy(cobalt_set_t* set);

/* Insert item (returns 0 on success, -1 on failure) */
int cobalt_set_insert(cobalt_set_t* set, void* item);

/* Remove item (returns 0 if found, -1 if not found) */
int cobalt_set_remove(cobalt_set_t* set, void* item);

/* Check if item exists */
int cobalt_set_contains(cobalt_set_t* set, void* item);

/* Get set size */
size_t cobalt_set_size(cobalt_set_t* set);

/* Check if empty */
int cobalt_set_is_empty(cobalt_set_t* set);

#endif /* SET_H */
```

**Implementation:** `src/container/set.c`
- Internal HashMap with NULL values
- Use item pointer as key
- Reuse existing hashmap functions

**Tests:** `tests/unit/test_set.c`
- Basic insert/remove/contains
- Empty set operations
- Duplicate insert (idempotent)
- Remove non-existent item
- NULL safety

---

### 3. New Container: Deque

**Rationale:** Deque (double-ended queue) is useful when both ends need O(1) operations. Existing list + stack + queue don't provide this combined functionality.

**Header:** `include/cobalt/container/deque.h`

```c
#ifndef DEQUE_H
#define DEQUE_H

#include <stddef.h>

typedef struct cobalt_deque cobalt_deque_t;

/* Create a new deque */
cobalt_deque_t* cobalt_deque_create(void);

/* Destroy the deque */
void cobalt_deque_destroy(cobalt_deque_t* deque);

/* Push to front */
int cobalt_deque_push_front(cobalt_deque_t* deque, void* item);

/* Push to back */
int cobalt_deque_push_back(cobalt_deque_t* deque, void* item);

/* Pop from front */
void* cobalt_deque_pop_front(cobalt_deque_t* deque);

/* Pop from back */
void* cobalt_deque_pop_back(cobalt_deque_t* deque);

/* Peek front without removing */
void* cobalt_deque_peek_front(cobalt_deque_t* deque);

/* Peek back without removing */
void* cobalt_deque_peek_back(cobalt_deque_t* deque);

/* Get size */
size_t cobalt_deque_size(cobalt_deque_t* deque);

/* Check if empty */
int cobalt_deque_is_empty(cobalt_deque_t* deque);

#endif /* DEQUE_H */
```

**Implementation:** `src/container/deque.c`
- Reuse list_node_t from list.c (or define locally)
- Maintain head and tail pointers
- O(1) for all operations

**Tests:** `tests/unit/test_deque.c`
- Push/pop from both ends
- Peek operations
- Size tracking
- Empty deque operations
- NULL safety

---

### 4. New Algorithms

**File:** `src/algorithm/functional.c` (extend)
**Header:** `include/cobalt/algorithm/functional.h` (extend)

```c
/* Binary search on sorted array */
void* cobalt_bsearch(const void* key, const void* base, size_t nmemb, 
                     size_t size, compare_func_t compar);

/* Find first element matching predicate */
void* cobalt_find_if(const void* base, size_t nmemb, size_t size,
                     predicate_func_t pred);

/* Apply operation to each element */
void cobalt_for_each(const void* base, size_t nmemb, size_t size,
                     operation_func_t op);
```

**Implementation:**
- `cobalt_bsearch`: Standard binary search returning pointer to element
- `cobalt_find_if`: Linear scan with predicate check
- `cobalt_for_each`: Simple loop calling operation on each element

**Tests:** `tests/unit/test_functional.c` (extend)
- Binary search on sorted array
- Find element with predicate
- For-each operation application

---

### 5. Windows IOCP Support

**File:** `src/module/eventloop.c` (extend)
**Header:** `include/cobalt/module/eventloop.h` (extend with Windows-specific API)

```c
#ifdef _WIN32
/* Windows-specific event loop operations */
int cobalt_eventloop_add_fd_win(cobalt_eventloop_t* loop, SOCKET socket,
                                 cobalt_events_t events, fd_handler_t callback,
                                 void* user_data);
#endif
```

**Implementation:**
- Add `#ifdef _WIN32` guards
- Use Windows Overlapped I/O with IOCP
- Maintain separate backend structures
- Keep Linux epoll/macOS kqueue as primary backends

**Note:** This is a significant addition. Consider deferring to Phase 4 if scope grows too large.

---

### 6. Compiler Optimization Hints

**Scope:** Selective application to hot paths

**Examples:**
```c
/* Restrict pointers for alias analysis */
void cobalt_vector_push(cobalt_vector_t* __restrict__ vec, void* __restrict__ item);

/* Hint inline for small functions */
__attribute__((always_inline))
static inline int is_empty(cobalt_vector_t* vec) {
    return vec->size == 0;
}

/* Branch prediction hint */
if (__builtin_expect(cobalt_vector_size(vec) == 0, 0)) {
    /* rare case */
}
```

**Implementation:**
- Add to critical path functions only
- Document optimization assumptions
- Test without hints to ensure no correctness issues

---

## Implementation Phases

### Phase 3a: Code Organization (Priority: High)
1. Split json.c into json_parse.c and json_serialize.c
2. Update CMakeLists.txt
3. Verify all tests pass

### Phase 3b: New Containers (Priority: High)
1. Implement Set container
2. Implement Deque container
3. Add tests for both

### Phase 3c: New Algorithms (Priority: Medium)
1. Implement binary_search, find_if, for_each
2. Add tests

### Phase 3d: Windows Support (Priority: Low)
1. Add IOCP backend (conditional compile)
2. Document Windows build requirements

### Phase 3e: Optimization Hints (Priority: Low)
1. Add restrict/inline hints to hot paths
2. Benchmark before/after

---

## Testing Strategy

Each new feature MUST have comprehensive tests:
- Set: 10+ test cases covering all operations
- Deque: 10+ test cases covering all operations
- Algorithms: 5+ test cases each
- JSON split: 0 regressions (all existing tests pass)

---

## Out of Scope for Phase 3

- Red-Black tree implementation for TreeMap (deferred)
- Custom allocator integration (deferred)
- Network I/O utilities (deferred to Phase 4)
- Full Windows build test (deferred)

---

## Success Criteria

- [ ] All Phase 3a tests pass
- [ ] Set container has 100% test coverage
- [ ] Deque container has 100% test coverage
- [ ] No regression in existing tests
- [ ] Code style consistent with existing codebase
- [ ] Documentation updated (headers, README)
