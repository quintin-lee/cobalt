# Cobalt Full Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the Cobalt C framework implementation from its architectural specification (RFC v2.0.0), fleshing out all stub implementations, adding comprehensive tests, and ensuring the build system produces a fully functional library.

**Architecture:** The Cobalt framework follows an 8-layer architecture with strict one-way dependencies: Layer 8 (Platform) → Layer 7a/b (Memory/Runtime) → Layer 6 (Object System) → Layer 5 (Interfaces) → Layer 4 (Collections) → Layer 3 (Algorithms) → Layer 2 (Modules) → Layer 1 (Applications). Each layer must be implemented bottom-up due to dependency constraints.

**Tech Stack:** C11 compiler (GCC/Clang), CMake 3.14+, CTest for testing, stdatomic.h for thread safety, Make/Ninja for build.

---

## Phase 1: Layer 8 — Platform & Kernel Abstraction (OAL)

### Task 1.1: Implement Platform Detection

**Files:**
- Create: `src/platform/platform.c` (stub with empty body → full impl)

Steps:
- [ ] **Step 1:** Define platform ID enum in `include/cobalt/platform/platform.h`:
  ```c
  typedef enum {
      COBALT_PLATFORM_UNKNOWN = 0,
      COBALT_PLATFORM_WINDOWS,
      COBALT_PLATFORM_MACOS,
      COBALT_PLATFORM_LINUX,
      COBALT_PLATFORM_OTHER
  } cobalt_platform_id_t;
  ```
- [ ] **Step 2:** Add detection macros using `#ifdef _WIN32`, `#ifdef __APPLE__`, etc.
- [ ] **Step 3:** Implement `cobalt_platform_get_id()` in `.c` with correct conditional compilation.
- [ ] **Step 4:** Implement `cobalt_platform_get_handle()` (return placeholder NULL for now).
- [ ] **Step 5:** Write unit test `tests/unit/test_platform.c` verifying return value on current OS.
- [ ] **Step 6:** Run test — ensure it passes. Commit with message `feat(platform): ✨ implement platform detection`.

### Task 1.2: Implement Atomic Operations

**Files:**
- Modify: `include/cobalt/platform/atomic.h`
- Create: `src/platform/atomic.c`

Steps:
- [ ] **Step 1:** Update `atomic.h` to use `_Atomic int` from `<stdatomic.h>`:
  ```c
  #include <stdatomic.h>
  typedef struct cobalt_atomic {
      atomic_int value;
  } cobalt_atomic_t;
  ```
- [ ] **Step 2:** Implement all five functions (`create`, `get`, `set`, `increment`, `decrement`) using atomic operations with appropriate memory ordering.
- [ ] **Step 3:** Write test `tests/unit/test_atomic.c` with multi-threaded increment/decrement verification.
- [ ] **Step 4:** Run test — verify thread-safe behavior without data races. Commit with message `feat(platform): ✨ implement atomic operations with stdatomic.h`.

### Task 1.3: Add Alignment Utilities (Future Extension)

**Files:**
- Create: `include/cobalt/platform/alignment.h` (placeholder, documented as future work)
- No implementation required yet per RFC spec.

---

## Phase 2: Layer 7a — Memory Subsystem

### Task 2.1: Implement Allocator Interface & System Allocator

**Files:**
- Modify: `include/cobalt/memory/allocator.h`
- Create: `src/memory/allocator.c`

Steps:
- [ ] **Step 1:** Define `cobalt_allocator_t` vtable struct in header (already exists but refine if needed).
- [ ] **Step 2:** Implement system allocator struct wrapping `malloc`/`free`/`realloc`.
- [ ] **Step 3:** Implement `cobalt_allocator_get_system()` returning pointer to static system allocator instance.
- [ ] **Step 4:** Implement helper allocators (`alloc`, `free`, `realloc`) that delegate to the vtable.
- [ ] **Step 5:** Write test verifying allocations go through system allocator (basic malloc/free cycle).
- [ ] **Step 6:** Commit with message `feat(memory): ✨ implement system allocator and allocator vtable`.

### Task 2.2: Implement Arena Allocator

**Files:**
- Modify: `include/cobalt/memory/arena.h`
- Create: `src/memory/arena.c`

Steps:
- [ ] **Step 1:** Define `cobalt_arena_t` struct in header with buffer pointer, size, used, capacity fields.
- [ ] **Step 2:** Implement `cobalt_arena_create(initial_size)` allocating buffer and initializing struct.
- [ ] **Step 3:** Implement `cobalt_arena_alloc(arena, size)` bump-pointer allocation with auto-growth via realloc.
- [ ] **Step 4:** Implement `cobalt_arena_reset(arena)` resetting used pointer to zero (instant free of all).
- [ ] **Step 5:** Implement `cobalt_arena_destroy(arena)` freeing the buffer and struct.
- [ ] **Step 6:** Write test verifying arena allocation works, reset frees memory, multiple allocations after reset succeed.
- [ ] **Step 7:** Commit with message `feat(memory): ✨ implement arena (region-based) allocator`.

---

## Phase 3: Layer 7b — Runtime Services

### Task 3.1: Implement Error Handling

**Files:**
- Modify: `include/cobalt/runtime/error.h`
- Create: `src/runtime/error.c`

Steps:
- [ ] **Step 1:** Ensure error code enum is complete with all defined values from RFC.
- [ ] **Step 2:** Implement `cobalt_error_get_message(code)` mapping each code to descriptive string.
- [ ] **Step 3:** Implement `cobalt_error_set(err_ptr, code)` writing to caller-provided variable.
- [ ] **Step 4:** Implement `cobalt_error_get_current()` — return `COBALT_SUCCESS` as placeholder (thread-local will be added later).
- [ ] **Step 5:** Test calling each function with various codes; verify messages match expectations.
- [ ] **Step 6:** Commit with message `feat(runtime): ✨ implement error handling subsystem`.

### Task 3.2: Implement Logger

**Files:**
- Modify: `include/cobalt/runtime/logger.h`
- Create: `src/runtime/logger.c`

Steps:
- [ ] **Step 1:** Define `log_level_t` enum with TRACE, DEBUG, INFO, WARNING, ERROR, FATAL.
- [ ] **Step 2:** Implement global `log_output` (FILE*) and `min_log_level` variables.
- [ ] **Step 3:** Implement `cobalt_logger_init(output_file, min_level)` setting globals.
- [ ] **Step 4:** Implement `level_name(level)` helper returning string abbreviation.
- [ ] **Step 5:** Implement `cobalt_logger_log(file, line, format, ...)` writing to output with formatting, using `vfprintf` and `va_list`.
- [ ] **Step 6:** Define macros `cobalt_trace()`, `cobalt_debug()`, `cobalt_info()`, `cobalt_warning()`, `cobalt_error()`, `cobalt_fatal()` injecting file/line.
- [ ] **Step 7:** Test logger at different levels, verify FATAL triggers exit(1).
- [ ] **Step 8:** Commit with message `feat(runtime): ✨ implement structured logging facility`.

---

## Phase 4: Layer 6 — Core Object System

### Task 4.1: Implement Object Base

**Files:**
- Modify: `include/cobalt/core/object.h`
- Create: `src/core/object.c`

Steps:
- [ ] **Step 1:** Confirm `cobalt_object_t` struct definition includes `ref_count` (uint64_t) and `class` pointer.
- [ ] **Step 2:** Implement `cobalt_object_ref(obj)` incrementing ref_count.
- [ ] **Step 3:** Implement `cobalt_object_unref(obj)` decrementing and freeing when count reaches zero (use atomic ops for production).
- [ ] **Step 4:** Implement `cobalt_object_new(cls, extra_size)` allocating sizeof(object)+extra with ref_count=1.
- [ ] **Step 5:** Implement `cobalt_object_get_class(obj)` returning the class pointer.
- [ ] **Step 6:** Test object creation, reference counting, and automatic cleanup.
- [ ] **Step 7:** Commit with message `feat(core): ✨ implement object base with reference counting`.

### Task 4.2: Implement Class System

**Files:**
- Modify: `include/cobalt/core/class.h`
- Create: `src/core/class.c`

Steps:
- [ ] **Step 1:** Define `cobalt_method_t` and `cobalt_property_t` structs as per RFC.
- [ ] **Step 2:** Define `cobalt_class_t` with name, method/property arrays, base_class, abstract flag.
- [ ] **Step 3:** Implement `cobalt_class_create(name, base_class)` allocating and initializing struct.
- [ ] **Step 4:** Implement `cobalt_class_add_method()` registering a method by name and function pointer.
- [ ] **Step 5:** Implement `cobalt_class_add_property()` registering getter/setter pair.
- [ ] **Step 6:** Implement `cobalt_class_is_abstract()` checking the abstract flag.
- [ ] **Step 7:** Implement `cobalt_class_destroy()` freeing name string, method/property arrays, and struct.
- [ ] **Step 8:** Test creating a class, adding a method, querying methods, destroying cleanly.
- [ ] **Step 9:** Commit with message `feat(core): ✨ implement class metadata and method registration`.

### Task 4.3: Implement Interface Mechanism

**Files:**
- Modify: `include/cobalt/core/interface.h`
- Create: `src/core/interface.c`

Steps:
- [ ] **Step 1:** Define `cobalt_interface_vtable_t` with destroy callback and interface-specific virtuals.
- [ ] **Step 2:** Define `cobalt_interface_t` with vtable pointer.
- [ ] **Step 3:** Implement `cobalt_interface_new(vtable)` allocating and setting vtable.
- [ ] **Step 4:** Implement `cobalt_interface_destroy(iface)` freeing the interface struct.
- [ ] **Step 5:** Implement `cobalt_object_implements(obj, iface)` checking if object supports this interface (stub: return 0 until runtime injection logic is added).
- [ ] **Step 6:** Implement `cobalt_object_query_interface(obj, iface_name)` searching object's interfaces for matching name (stub: return NULL).
- [ ] **Step 7:** Test interface creation and basic query.
- [ ] **Step 8:** Commit with message `feat(core): ✨ implement multi-interface mechanism (COM-style QueryInterface)`.

---

## Phase 5: Layer 5 — Container Interfaces

### Task 5.1: Implement Sequence Interface

**Files:**
- Modify: `include/cobalt/interface/sequence.h`

Steps:
- [ ] **Step 1:** Define `cobalt_sequence_ops` vtable with size, is_empty, add, remove, iterator function pointers.
- [ ] **Step 2:** Define `cobalt_sequence_t` embedding the ops pointer.
- [ ] **Step 3:** Create prototype declaration for `cobalt_sequence_create(capacity)` and `cobalt_sequence_destroy(seq)`.
- [ ] **Step 4:** No implementation here (interfaces are pure abstract); concrete types will implement these ops.
- [ ] **Step 5:** Document in header that sequences are first members of concrete container types.
- [ ] **Step 6:** Commit with message `feat(interface): ✨ define Sequence interface vtable contract`.

### Task 5.2: Implement Map Interface

**Files:**
- Modify: `include/cobalt/interface/map.h`

Steps:
- [ ] **Step 1:** Define `cobalt_map_ops` with get, put, remove, size, is_empty function pointers.
- [ ] **Step 2:** Define `cobalt_map_t` embedding the ops pointer.
- [ ] **Step 3:** Declare `cobalt_map_create()`, `cobalt_map_destroy(m)`.
- [ ] **Step 4:** Commit with message `feat(interface): ✨ define Map interface vtable contract`.

### Task 5.3: Implement Iterator Interface

**Files:**
- Modify: `include/cobalt/interface/iterator.h`

Steps:
- [ ] **Step 1:** Define `cobalt_iterator_t` with functions `has_next()`, `next()`, `destroy()`.
- [ ] **Step 2:** Create concrete iterator implementations that will be provided by containers.
- [ ] **Step 3:** Commit with message `feat(interface): ✨ define Iterator traversal protocol`.

---

## Phase 6: Layer 4 — Concrete Collections

### Task 6.1: Implement Vector (Dynamic Array)

**Files:**
- Modify: `include/cobalt/container/vector.h`
- Create: `src/container/vector.c`

Steps:
- [ ] **Step 1:** Define `cobalt_vector_t` embedding `cobalt_sequence_t` as first member (for polymorphism), plus items array, capacity, size fields.
- [ ] **Step 2:** Implement vector-specific ops struct implementing Sequence interface (size, is_empty, add, remove, iterator).
- [ ] **Step 3:** Implement `cobalt_vector_create(initial_capacity)` allocating struct and items array.
- [ ] **Step 4:** Implement `cobalt_vector_destroy(vec)` freeing items and struct.
- [ ] **Step 5:** Implement `cobalt_vector_push(vec, item)` appending with auto-resize.
- [ ] **Step 6:** Implement `cobalt_vector_get(vec, index)` with bounds check.
- [ ] **Step 7:** Implement `cobalt_vector_set(vec, index, item)` updating element.
- [ ] **Step 8:** Implement `cobalt_vector_size()` and `is_empty()`.
- [ ] **Step 9:** Write test verifying push, get, set, size operations work correctly.
- [ ] **Step 10:** Commit with message `feat(container): ✨ implement Vector dynamic array (Sequence impl)`.

### Task 6.2: Implement List (Doubly-Linked)

**Files:**
- Modify: `include/cobalt/container/list.h`
- Create: `src/container/list.c`

Steps:
- [ ] **Step 1:** Define `cobalt_list_node_t` with data, next, prev pointers.
- [ ] **Step 2:** Define `cobalt_list_t` embedding `cobalt_sequence_t`, head, tail, size.
- [ ] **Step 3:** Implement list-specific ops for Sequence interface.
- [ ] **Step 4:** Implement `cobalt_list_create()`, `destroy()`, `push_front()`, `push_back()`, `pop_front()`, `get()`, `size()`, `is_empty()`.
- [ ] **Step 5:** Test list operations, especially edge cases (empty list, single node).
- [ ] **Step 6:** Commit with message `feat(container): ✨ implement Doubly-linked List (Sequence impl)`.

### Task 6.3: Implement HashMap (Chained Hash Table)

**Files:**
- Modify: `include/cobalt/container/hashmap.h`
- Create: `src/container/hashmap.c`

Steps:
- [ ] **Step 1:** Define `cobalt_hashmap_node_t` with key (char*), value (void*), next for collision chaining.
- [ ] **Step 2:** Define `cobalt_hashmap_t` embedding `cobalt_map_t`, buckets array, bucket_count, size.
- [ ] **Step 3:** Implement hash function for string keys (djb2 or similar).
- [ ] **Step 4:** Implement `cobalt_hashmap_create(initial_buckets)`.
- [ ] **Step 5:** Implement `put(key, value)` hashing to bucket, inserting at head of chain.
- [ ] **Step 6:** Implement `get(key)` searching chain for matching key.
- [ ] **Step 7:** Implement `remove(key)` removing from chain, updating size.
- [ ] **Step 8:** Implement `size()`, `is_empty()`.
- [ ] **Step 9:** Test insert/lookup/remove of multiple entries, handle collisions.
- [ ] **Step 10:** Commit with message `feat(container): ✨ implement HashMap with string keys (Map impl)`.

### Task 6.4: Implement TreeMap (Red-Black Tree)

**Files:**
- Modify: `include/cobalt/container/treemap.h`
- Create: `src/container/treemap.c`

Steps:
- [ ] **Step 1:** Define `cobalt_tree_node_t` with key, value, color (RED/BLACK), left, right, parent.
- [ ] **Step 2:** Define `cobalt_treemap_t` embedding `cobalt_map_t`, root, size.
- [ ] **Step 3:** Implement tree rotation helpers (left_rotate, right_rotate).
- [ ] **Step 4:** Implement insert fixup maintaining red-black properties.
- [ ] **Step 5:** Implement `treemap_create()`, `put(key,value)`, `get(key)`, `remove(key)`, `min_key()`, `max_key()`, `size()`.
- [ ] **Step 6:** Test inserting multiple keys, verify sorted order via min/max/get, verify tree remains balanced.
- [ ] **Step 7:** Note: Full RB-tree implementation is complex; consider starting with simpler BST then optimizing, or use placeholder stub with TODO comments and integrate complete implementation later.
- [ ] **Step 8:** Commit with message `feat(container): ✨ implement TreeMap (Map impl with sorted iteration)` — mark as work-in-progress if stub.

---

## Phase 7: Layer 3 — Algorithms & Functional Streams

### Task 7.1: Implement Sorting Algorithms

**Files:**
- Modify: `include/cobalt/algorithm/sort.h`
- Create: `src/algorithm/sort.c`

Steps:
- [ ] **Step 1:** Define `compare_func_t` typedef for comparison callbacks.
- [ ] **Step 2:** Implement `cobalt_qsort(base, nmemb, size, compar)` wrapper around standard `qsort()`.
- [ ] **Step 3:** Implement `cobalt_insertion_sort()` for small arrays (simple nested loop).
- [ ] **Step 4:** Implement `cobalt_list_sort(head, count, compar)` merge-sort over linked lists (stub if complex, implement fully if feasible).
- [ ] **Step 5:** Write test sorting integer arrays with custom comparator; verify sorted output.
- [ ] **Step 6:** Commit with message `feat(algorithm): ✨ implement generic sorting algorithms (qsort, insertion, merge)`.

### Task 7.2: Implement Functional Predicates

**Files:**
- Modify: `include/cobalt/algorithm/functional.h`
- Create: `src/algorithm/functional.c`

Steps:
- [ ] **Step 1:** Define `predicate_func_t` and `operation_func_t` typedefs.
- [ ] **Step 2:** Define `function_obj_t` struct with context and apply callback.
- [ ] **Step 3:** Implement `predicate_equal(a, b, comp)` using three-way comparison.
- [ ] **Step 4:** Implement `predicate_not_equal(a, b, comp)` negating equal.
- [ ] **Step 5:** Implement `predicate_null(item)` and `predicate_nonnull(item)` checks.
- [ ] **Step 6:** Test predicates against NULL/non-NULL and equal/not-equal pairs.
- [ ] **Step 7:** Commit with message `feat(algorithm): ✨ implement common predicate functions for filtering/searching`.

---

## Phase 8: Layer 2 — Modules & Utilities

### Task 8.1: Implement JSON Module

**Files:**
- Modify: `include/cobalt/module/json.h`
- Create: `src/module/json.c`

Steps:
- [ ] **Step 1:** Define `json_type_t` enum and `json_value_t` union.
- [ ] **Step 2:** Define `json_node_t` tree structure with type, value, next (for children), key (for object fields).
- [ ] **Step 3:** Implement `json_parse(text)` — stub for now (could implement simple parser later; at minimum create basic node constructors).
- [ ] **Step 4:** Implement `json_serialize(node)` generating JSON string from tree (stub or recursive serializer).
- [ ] **Step 5:** Implement `json_destroy(node)` recursively freeing all nodes.
- [ ] **Step 6:** Implement accessors: `get_number()`, `get_string()`, `is_null/is_object/is_array()`.
- [ ] **Step 7:** Test parsing a simple JSON object like `{"name":"Alice","age":30}` and extracting values.
- [ ] **Step 8:** Commit with message feat(module): ✨ implement JSON serialization/deserialization tree API (work-in-progress if parser is stub).

### Task 8.2: Implement Event Loop

**Files:**
- Modify: `include/cobalt/module/eventloop.h`
- Create: `src/module/eventloop.c`

Steps:
- [ ] **Step 1:** Define `fd_handler_t` and `timer_handler_t` callback types.
- [ ] **Step 2:** Define `cobalt_eventloop_t` struct with backend-specific fields (epoll fd on Linux, kqueue on macOS, stub for Windows).
- [ ] **Step 3:** Implement `eventloop_create()` allocating struct, initializing platform-specific backend.
- [ ] **Step 4:** Implement `eventloop_destroy()` freeing resources.
- [ ] **Step 5:** Implement `add_fd(loop, fd, events, cb, ctx)` registering FD handler.
- [ ] **Step 6:** Implement `mod_fd()` and `del_fd()` variants.
- [ ] **Step 7:** Implement `add_timer()` with monotonically increasing IDs.
- [ ] **Step 8:** Implement `del_timer()` canceling timers.
- [ ] **Step 9:** Implement `run(blocking)` and `stop()` controlling main loop.
- [ ] **Step 10:** Implement `iteration()` non-blocking step.
- [ ] **Step 11:** Test basic event loop creation/destruction; simulate timer firing (stub timer logic).
- [ ] **Step 12:** Commit with message feat(module): ✨ implement event loop with FD and timer callbacks (platform-backend stubbed).

---

## Phase 9: Integration & Master Header

### Task 9.1: Consolidate Master Header

**Files:**
- Modify: `include/cobalt/cobalt.h`

Steps:
- [ ] **Step 1:** Verify all included headers are present in the correct dependency order (L8→L2).
- [ ] **Step 2:** Add include guard `#ifndef COBALT_H` / `#define COBALT_H` / `#endif`.
- [ ] **Step 3:** Ensure no circular includes exist.
- [ ] **Step 4:** Add brief Doxygen-style comment at top describing purpose.
- [ ] **Step 5:** Commit with message feat(core): ✨ consolidate master cobalt.h header with proper include ordering.

### Task 9.2: Link All Source into Library

**Files:**
- Modify: `CMakeLists.txt`

Steps:
- [ ] **Step 1:** Ensure all .c files in `src/` are listed in the `SOURCES` variable.
- [ ] **Step 2:** Verify target_include_directories sets PUBLIC include path.
- [ ] **Step 3:** Add compile options: `-Wall -Wextra -Wpedantic` and enforce missing warnings.
- [ ] **Step 4:** Enable CTest and add a dummy `test` target that runs `make test`.
- [ ] **Step 5:** Commit with message feat(build): ✨ complete CMake configuration for library build and testing.

---

## Phase 10: Testing Infrastructure

### Task 10.1: Set Up Unit Test Framework

**Files:**
- Create: `tests/unit/test_runner.h` and `.c`
- Create: `tests/CMakeLists.txt`

Steps:
- [ ] **Step 1:** Choose a lightweight test framework (e.g., minimal custom assert macros + registry, or integrate tinytest).
- [ ] **Step 2:** Define test macros: `TEST_FUNC(name){ ... body ... }`, `RUN_ALL_TESTS()`.
- [ ] **Step 3:** Create `tests/unit/test_registry.c` collecting all test registrations.
- [ ] **Step 4:** Write CMake config to build `cobalt_test` executable linking all unit tests.
- [ ] **Step 5:** Add `enable_testing()` and `add_test(NAME CobaltUnitTest COMMAND cobalt_test)` to top-level CMakeLists.txt.
- [ ] **Step 6:** Commit with message feat(test): ✨ set up unit test infrastructure with CTest integration.

### Task 10.2: Write Tests for All Modules

Organized by module:

| Module | Test File | Coverage |
|--------|-----------|----------|
| Platform | `tests/unit/test_platform.c` | platform detection |
| Atomic | `tests/unit/test_atomic.c` | atomic inc/dec, thread safety |
| Allocator | `tests/unit/test_allocator.c` | malloc/free cycle |
| Arena | `tests/unit/test_arena.c` | alloc/reset/destroy lifecycle |
| Error | `tests/unit/test_error.c` | message lookup, set/get |
| Logger | `tests/unit/test_logger.c` | log level filtering, macro injection |
| Object | `tests/unit/test_object.c` | new/ref/unref/lifecycle |
| Class | `tests/unit/test_class.c` | create/add_method/destroy |
| Interface | `tests/unit/test_interface.c` | interface creation/query |
| Vector | `tests/unit/test_vector.c` | push/get/set/size/destroy |
| List | `tests/unit/test_list.c` | push/pop/front/back/get/size |
| HashMap | `tests/unit/test_hashmap.c` | put/get/remove/size |
| TreeMap | `tests/unit/test_treemap.c*` | put/get/remove/size/order (*if implemented) |
| Sort | `tests/unit/test_sort.c` | qsort/comparator correctness |
| JSON | `tests/unit/test_json.c*` | parse/serialize/destroy (*if implemented) |

- [ ] **Step 1:** For each test file, write tests covering typical use cases and edge cases (NULL inputs, empty collections, boundary conditions).
- [ ] **Step 2:** Add each test file to `tests/unit/CMakeLists.txt` (or include via glob).
- [ ] **Step 3:** Run `ctest --output-on-failure` to verify all tests pass.
- [ ] **Step 4:** Commit all test files with message feat(test): ✨ add comprehensive unit tests for core objects, containers, and algorithms.

---

## Phase 11: Example Programs & Documentation Updates

### Task 11.1: Compile and Validate Examples

**Files:**
- Verify all `.c` files in `docs/examples/` compile successfully.

Steps:
- [ ] **Step 1:** For each example source file, attempt to compile it manually to confirm no missing includes or undefined symbols.
- [ ] **Step 2:** Fix any compilation errors (missing includes, typos, missing function prototypes).
- [ ] **Step 3:** Ensure examples link correctly against the built library.
- [ ] **Step 4:** Update `docs/EXAMPLES/README.md` with actual compile commands for each example category.
- [ ] **Step 5:** Commit with message feat(docs): ✨ validate and fix all example programs to compile against current API.

### Task 11.2: Update Developer Guide

**Files:**
- Modify: `docs/DEV_GUIDE/DEVELOPER_GUIDE.md`

Steps:
- [ ] **Step 1:** Verify coding standards in guide match actual implementation (e.g., naming conventions, include guard pattern).
- [ ] **Step 2:** Add section explaining how to extend the framework with new classes/modules following existing patterns.
- [ ] **Step 3:** Add troubleshooting FAQ for common build errors.
- [ ] **Step 4:** Commit with message feat(docs): ✨ update developer guide with implementation specifics.

---

## Phase 12: Release Preparation

### Task 12.1: Final Build & Smoke Test

Steps:
- [ ] **Step 1:** Clean build: `rm -rf build && mkdir build && cd build && cmake .. && make`
- [ ] **Step 2:** Run tests: `make test` or `ctest` — all must pass.
- [ ] **Step 3:** Install to staging: `make install DESTDIR=/tmp/stage` verify files placed correctly.
- [ ] **Step 4:** Use pkg-config: `pkg-config --libs cobalt` and `pkg-config --cflags cobalt` verify they work.
- [ ] **Step 5:** Commit with message feat(build): ✨ final smoke test and packaging validation.

### Task 12.2: Tag Release Version

Steps:
- [ ] **Step 1:** Create Git tag: `git tag -a v2.0.0 -m "Cobalt v2.0.0 Architecture Baseline release"`
- [ ] **Step 2:** Push tags: `git push origin v2.0.0`
- [ ] **Step 3:** Update CHANGELOG.md to mark v2.0.0 as released (add date, status "Released").
- [ ] **Step 4:** Commit with message chore(release): 📦 tag v2.0.0 release and update changelog.

---

## Plan Verification Checklist

| Check | Item | Status |
|-------|------|--------|
| 1 | All Layer 8 (Platform) functions fully implemented with working tests | ☐ |
| 2 | All Layer 7a (Memory) allocators implemented with leak-free tests | ☐ |
| 3 | All Layer 7b (Runtime) error/logger services operational | ☐ |
| 4 | Core Object System (Object/Class/Interface) fully functional | ☐ |
| 5 | Container Interfaces (Sequence/Map/Iterator) properly defined | ☐ |
| 6 | All Collection types (Vector/List/HashMap/Treemap) implemented with tests | ☐ |
| 7 | Algorithms (Sort/Functional) correctly operate on interfaces | ☐ |
| 8   |   Modules (JSON/EventLoop) integrated and tested |   ☐ |
| 9   |   Master header `cobalt.h` exposes complete public API |   ☐ |
| 10  |   CMake builds library, runs tests, generates pkg-config |   ☐ |
| 11  |   All examples compile and demonstrate usage |   ☐ |
| 12  |   Developer Guide accurately reflects implementation details |   ☐ |
| 13  |   Release tag v2.0.0 created with changelog |   ☐ |

**Review before implementation start:** This plan covers every module mentioned in the SPEC documents and implements the full 8-layer architecture as described in RFC v2.0.0. Tasks are ordered by dependency (lower layers before higher layers) so each phase can stand alone as a working milestone.
