# Cobalt Quality Fixes — Specification

**Goal:** Fix four remaining quality issues identified in the codebase analysis.

---

## Issue 1 — List malloc failure silently swallowed (Critical)

**File:** `src/container/list.c`, line 68–71

**Problem:** `list_add_seq` allocates a node with `malloc`. On failure, it returns without calling `cobalt_error_set()`. Callers have no way to detect the OOM.

**Fix:** Add `cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY)` before the early return.

**After:**
```c
list_node_t *node = malloc(sizeof(list_node_t));
if (!node) {
    cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
    return;
}
```

**Test:** `tests/unit/test_list.c` already covers OOM paths for push_front/push_back. Add a test that calls the sequence interface's `add` directly (via `seq->add`) and verifies the error code is set on malloc failure. Since simulating malloc failure is hard, verify by code inspection that the pattern matches vector's implementation.

---

## Issue 2 — `cobalt_hashmap_node` exposed publicly (Medium)

**File:** `include/cobalt/container/hashmap.h`, lines 22–34

**Problem:** The `cobalt_hashmap_node` struct is defined in the public header. Users can access `node->key`, `node->value`, `node->next` directly, breaking encapsulation. No code in `examples/` or `tests/` uses it directly.

**Fix:**
1. In `hashmap.h`: change `struct cobalt_hashmap_node { ... };` to opaque forward declaration `typedef struct cobalt_hashmap_node cobalt_hashmap_node_t;`
2. Move the struct definition into `src/container/hashmap.c`
3. No API function changes needed — the node is never returned to callers

**After in hashmap.h:**
```c
typedef struct cobalt_hashmap_node cobalt_hashmap_node_t;
```
(The rest of the header — `cobalt_hashmap_t` typedef and all public functions — stays unchanged.)

**After in hashmap.c:** Add `struct cobalt_hashmap_node { ... };` after the existing `hashmap_impl_t` typedef, same fields as before.

---

## Issue 3 — `json.c` includes `.c` files (Medium)

**File:** `src/module/json.c`

**Problem:**
```c
#include "json_parse.c"
#include "json_serialize.c"
```
Both files are already listed in `CMakeLists.txt` as separate sources. The `#include` is redundant and unusual in C.

**Fix:** Remove the two `#include` lines from `json.c`. The query/helper functions (`json_get_number`, `json_tree_get_child`, etc.) are only declared in `json.h` — their implementations are in `json_parse.c` and `json_serialize.c`, which CMake compiles separately.

**Note:** Check that all helper functions used in `json.c` are declared in `json.h` or that they can be accessed via the included headers. If not, they need to be declared static in `json.c` or the include is needed. After review: the functions in `json.c` are top-level (non-static) and declared in `json.h`, so removing the `.c` includes is safe.

---

## Issue 4 — List value-based remove (Low)

**File:** `src/container/list.c` + `include/cobalt/container/list.h`

**Problem:** `cobalt_list_remove()` compares by pointer equality only. There's no way to remove by value using a comparator function.

**Fix:** Add `cobalt_list_remove_if(cobalt_list_t *list, int (*predicate)(const void *item, void *user_data), void *user_data)` — removes the first element for which `predicate(item, user_data)` returns non-zero.

**API in list.h:**
```c
/**
 * @brief Remove the first element matching a predicate
 * @param list Pointer to the target list
 * @param predicate Function that returns non-zero for matching elements
 * @param user_data Opaque pointer passed to the predicate
 * @return Returns 0 on success; -1 if list is NULL or no element matches
 */
int cobalt_list_remove_if(cobalt_list_t *list,
                          int (*predicate)(const void *item, void *user_data),
                          void *user_data);
```

**Implementation:** Same traversal as `cobalt_list_remove`, but call `predicate(item, user_data)` instead of `node->data == item`.

**Test:** Add `test_list_remove_if` to `tests/unit/test_list.c` that verifies removal of a specific value using a predicate.

---

## Verification

All four fixes must pass:
```bash
cd build && cmake --build . --parallel && ctest --output-on-failure
```
Expected: 22/22 tests pass, zero new failures.
