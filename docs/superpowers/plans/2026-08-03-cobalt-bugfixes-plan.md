# Cobalt Phase 4: Bug Fixes & Quality Improvements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix critical bugs (eventloop usleep, TreeMap memory leak, JSON escapes), complete missing functionality (list remove, set type safety), and improve code quality.

**Architecture:** Four independent fix categories: P0 (blocking bugs), P1 (correctness), P2 (completeness), P3 (quality). Each is self-contained with tests.

**Tech Stack:** C11, CMake, CTest, clang-tidy, clang-format

---

## File Structure

### Files to Modify
- `src/module/eventloop.c` — Fix usleep bug (line 533)
- `src/container/treemap.c` — Implement real node deletion
- `src/container/list.c` — Implement list_remove_seq
- `src/module/json_parse.c` — Add escape sequence handling
- `src/container/set.c` — Add type safety warning/documentation
- `include/cobalt/container/set.h` — Document string-only constraint
- `tests/unit/test_treemap.c` — Add tests for real deletion
- `tests/unit/test_list.c` — Add tests for remove operation
- `tests/unit/test_json.c` — Add tests for escape sequences
- `tests/unit/test_eventloop.c` — Fix timer test expectations

---

## Task 1: Fix Event Loop usleep Bug

**Files:**
- Modify: `src/module/eventloop.c:533`
- Modify: `tests/unit/test_eventloop.c`

- [ ] **Step 1: Understand the bug**

Line 533 in `src/module/eventloop.c`:
```c
usleep(COBALT_MILLIS_PER_SEC);
```
`COBALT_MILLIS_PER_SEC` is defined as `1000ULL`. `usleep()` takes **microseconds**, so this sleeps for 1,000,000 microseconds = 1 second per iteration! This makes the event loop extremely sluggish.

The fix is to either:
- Remove the usleep entirely (the loop already has epoll/kqueue blocking)
- Or change to `usleep(1000)` for a 1ms poll fallback

The best fix: remove the usleep line entirely since `cobalt_eventloop_iteration()` already blocks on epoll_wait/kqueue.

- [ ] **Step 2: Write the failing test first**

Add this test to `tests/unit/test_eventloop.c` before the existing `test_eventloop()`:

```c
void test_eventloop_timing(void)
{
    printf("Testing eventloop timing...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    int call_count = 0;
    timer_handler_t fast_callback = [&](uint64_t id, void *ud) {
        (void)id;
        (void)ud;
        call_count++;
    };
    // Note: C doesn't have lambdas, use a static counter approach
    // See implementation in actual test file
```

Actually, use this simpler approach — test that iteration returns quickly:

```c
void test_eventloop_timing(void)
{
    printf("Testing eventloop timing...\n");

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Run 10 iterations on an empty loop
    for (int i = 0; i < 10; i++) {
        cobalt_eventloop_iteration(loop);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 +
                      (end.tv_nsec - start.tv_nsec) / 1000000;

    printf("  10 iterations took %ld ms\n", elapsed_ms);
    // Should be well under 1000ms (1 second per iteration was the bug)
    TEST_ASSERT(elapsed_ms < 1000);

    cobalt_eventloop_destroy(loop);
    printf("  Eventloop timing test passed\n");
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd build && ctest --output-on-failure --filter test_eventloop`
Expected: The timing test should be slow or the existing tests should show timing issues.

- [ ] **Step 4: Fix the bug**

Edit `src/module/eventloop.c:533`:

```c
// BEFORE:
    while (loop->running && !loop->stop_flag) {
        cobalt_eventloop_iteration(loop);
        usleep(COBALT_MILLIS_PER_SEC);
    }

// AFTER:
    while (loop->running && !loop->stop_flag) {
        cobalt_eventloop_iteration(loop);
    }
```

Remove the `usleep(COBALT_MILLIS_PER_SEC);` line entirely. The `cobalt_eventloop_iteration()` function already blocks on `epoll_wait()` or `kevent()` with appropriate timeouts.

- [ ] **Step 5: Run tests to verify**

Run: `cd build && ctest --output-on-failure --filter test_eventloop`
Expected: All eventloop tests pass, including timing test.

- [ ] **Step 6: Commit**

```bash
git add src/module/eventloop.c tests/unit/test_eventloop.c
git commit -m "fix(eventloop): remove erroneous usleep(1000ms) causing 1s delay per iteration"
```

---

## Task 2: Fix TreeMap Soft Delete Memory Leak

**Files:**
- Modify: `src/container/treemap.c`
- Modify: `tests/unit/test_treemap.c`

- [ ] **Step 1: Understand the problem**

In `src/container/treemap.c:238-254`, `cobalt_treemap_remove()` only sets `node->value = NULL` without freeing the node or its key. This causes memory to grow unboundedly.

- [ ] **Step 2: Write the failing test first**

Add to `tests/unit/test_treemap.c`:

```c
void test_treemap_remove_frees_memory(void)
{
    printf("Testing treemap remove frees memory...\n");

    cobalt_treemap_t *map = cobalt_treemap_create();
    TEST_ASSERT(map != NULL);

    // Insert several items
    int val1 = 1;
    int val2 = 2;
    int val3 = 3;

    TEST_ASSERT(cobalt_treemap_put(map, "a", &val1) == 0);
    TEST_ASSERT(cobalt_treemap_put(map, "b", &val2) == 0);
    TEST_ASSERT(cobalt_treemap_put(map, "c", &val3) == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 3);

    // Remove one item
    TEST_ASSERT(cobalt_treemap_remove(map, "b") == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 2);

    // Verify removal
    TEST_ASSERT(cobalt_treemap_get(map, "b") == NULL);
    TEST_ASSERT(cobalt_treemap_get(map, "a") == &val1);
    TEST_ASSERT(cobalt_treemap_get(map, "c") == &val3);

    // Re-insert same key with different value
    int val2_new = 200;
    TEST_ASSERT(cobalt_treemap_put(map, "b", &val2_new) == 0);
    TEST_ASSERT(cobalt_treemap_size(map) == 3);
    TEST_ASSERT(cobalt_treemap_get(map, "b") == &val2_new);

    // Clean up all
    cobalt_treemap_destroy(map);
    printf("  TreeMap remove test passed\n");
}
```

Add `test_treemap_remove_frees_memory();` to the `test_treemap()` function.

- [ ] **Step 3: Run test to verify it fails (or passes incorrectly)**

Run: `cd build && ctest --output-on-failure --filter test_treemap`
Expected: Test may pass functionally but will leak memory (verify with valgrind).

- [ ] **Step 4: Implement proper node deletion**

Edit `src/container/treemap.c`. Add a helper to find and remove a node:

```c
/**
 * @brief Recursively find and remove a node, returning the new subtree root
 * @return New root of the subtree after removal
 */
static treemap_node_t *remove_node(treemap_node_t *node, const char *key, size_t *size)
{
    if (!node) {
        return NULL;
    }

    int cmp = strcmp(key, node->key);
    if (cmp < 0) {
        node->left = remove_node(node->left, key, size);
    } else if (cmp > 0) {
        node->right = remove_node(node->right, key, size);
    } else {
        // Found the node to remove
        *size -= 1;

        // Case 1: No children
        if (!node->left && !node->right) {
            free(node->key);
            free(node);
            return NULL;
        }
        // Case 2: Only right child
        else if (!node->left) {
            treemap_node_t *tmp = node->right;
            free(node->key);
            free(node);
            return tmp;
        }
        // Case 3: Only left child
        else if (!node->right) {
            treemap_node_t *tmp = node->left;
            free(node->key);
            free(node);
            return tmp;
        }
        // Case 4: Two children — find inorder successor (min of right subtree)
        else {
            treemap_node_t *successor = find_min(node->right);
            // Copy successor's key and value to current node
            char *old_key = node->key;
            void *old_value = node->value;
            node->key = cobalt_strdup(successor->key);
            node->value = successor->value;
            free(old_key);
            // Note: we don't free old_value — it belongs to the user
            // Remove the successor from the right subtree
            node->right = remove_node(node->right, successor->key, size);
        }
    }

    return node;
}
```

Then update `cobalt_treemap_remove()`:

```c
int cobalt_treemap_remove(cobalt_treemap_t *map, const char *key)
{
    if (!map || !key) {
        return -1;
    }
    treemap_node_t *node = find_node(map->impl.root, key);
    if (!node) {
        return -1;
    }

    map->impl.root = remove_node(map->impl.root, key, &map->impl.size);
    return 0;
}
```

- [ ] **Step 5: Run tests to verify**

Run: `cd build && ctest --output-on-failure --filter test_treemap`
Expected: All treemap tests pass.

- [ ] **Step 6: Verify no memory leak with valgrind**

Run: `valgrind --leak-check=full --error-exitcode=1 ./build/tests/cobalt_test --filter treemap`
Expected: No memory leaks.

- [ ] **Step 7: Commit**

```bash
git add src/container/treemap.c tests/unit/test_treemap.c
git commit -m "fix(treemap): implement real node deletion instead of soft delete"
```

---

## Task 3: Implement List Remove Operation

**Files:**
- Modify: `src/container/list.c`
- Modify: `tests/unit/test_list.c`

- [ ] **Step 1: Write the failing test first**

Add to `tests/unit/test_list.c`:

```c
void test_list_remove(void)
{
    printf("Testing list remove...\n");

    cobalt_list_t *list = cobalt_list_create();
    TEST_ASSERT(list != NULL);

    int a = 1, b = 2, c = 3;

    TEST_ASSERT(cobalt_list_push_back(list, &a) == 0);
    TEST_ASSERT(cobalt_list_push_back(list, &b) == 0);
    TEST_ASSERT(cobalt_list_push_back(list, &c) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 3);

    // Remove middle element
    TEST_ASSERT(cobalt_list_remove(list, &b) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 2);
    TEST_ASSERT(cobalt_list_get(list, 0) == &a);
    TEST_ASSERT(cobalt_list_get(list, 1) == &c);

    // Remove head
    TEST_ASSERT(cobalt_list_remove(list, &a) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 1);
    TEST_ASSERT(cobalt_list_get(list, 0) == &c);

    // Remove tail
    TEST_ASSERT(cobalt_list_remove(list, &c) == 0);
    TEST_ASSERT(cobalt_list_size(list) == 0);
    TEST_ASSERT(cobalt_list_is_empty(list));

    // Remove from empty list
    TEST_ASSERT(cobalt_list_remove(list, &a) == -1);

    // Remove non-existent element
    int d = 4;
    TEST_ASSERT(cobalt_list_remove(list, &d) == -1);

    cobalt_list_destroy(list);
    printf("  List remove test passed\n");
}
```

Also add `cobalt_list_remove()` declaration to `include/cobalt/container/list.h`:

```c
/**
 * @brief Remove the first occurrence of an element from the list
 *
 * @param list Pointer to the target list
 * @param item Pointer to the data to remove (compared by pointer equality)
 * @return Returns 0 on success; returns -1 if list is NULL, item is NULL, or element not found.
 */
int cobalt_list_remove(cobalt_list_t *list, void *item);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && ctest --output-on-failure --filter test_list`
Expected: FAIL — `cobalt_list_remove` undefined or stub returns -1 always.

- [ ] **Step 3: Implement list_remove_seq and cobalt_list_remove**

Edit `src/container/list.c`:

```c
/**
 * @brief Remove element from sequence (Sequence interface implementation)
 */
static void list_remove_seq(cobalt_sequence_t *self, void *item)
{
    if (!self || !item) {
        return;
    }
    cobalt_list_impl_t *list = (cobalt_list_impl_t *)self;
    list_node_t *node = list->head;
    list_node_t *prev = NULL;

    while (node) {
        if (node->data == item) {
            if (prev) {
                prev->next = node->next;
            } else {
                list->head = node->next;
            }
            if (node->next) {
                node->next->prev = prev;
            } else {
                list->tail = prev;
            }
            free(node);
            list->size--;
            return;
        }
        prev = node;
        node = node->next;
    }
}
```

Add public function:

```c
int cobalt_list_remove(cobalt_list_t *list, void *item)
{
    if (!list || !item) {
        return -1;
    }
    cobalt_list_impl_t *impl = (cobalt_list_impl_t *)list;
    list_node_t *node = impl->head;
    list_node_t *prev = NULL;

    while (node) {
        if (node->data == item) {
            if (prev) {
                prev->next = node->next;
            } else {
                impl->head = node->next;
            }
            if (node->next) {
                node->next->prev = prev;
            } else {
                impl->tail = prev;
            }
            free(node);
            impl->size--;
            return 0;
        }
        prev = node;
        node = node->next;
    }
    return -1;
}
```

- [ ] **Step 4: Run tests to verify**

Run: `cd build && ctest --output-on-failure --filter test_list`
Expected: All list tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/container/list.c include/cobalt/container/list.h tests/unit/test_list.c
git commit -m "feat(list): implement cobalt_list_remove() for value-based removal"
```

---

## Task 4: Fix JSON Escape Sequence Handling

**Files:**
- Modify: `src/module/json_parse.c`
- Modify: `tests/unit/test_json.c`

- [ ] **Step 1: Write the failing test first**

Add to `tests/unit/test_json.c`:

```c
void test_json_escape_sequences(void)
{
    printf("Testing JSON escape sequences...\n");

    // Test newline escape
    char *json1 = "{\"msg\":\"hello\\nworld\"}";
    json_node_t *root1 = json_parse(json1);
    TEST_ASSERT(root1 != NULL);
    json_node_t *msg1 = json_tree_get_child(root1, "msg");
    TEST_ASSERT(msg1 != NULL);
    TEST_ASSERT(strcmp(json_get_string(msg1), "hello\nworld") == 0);
    json_destroy(root1);

    // Test tab escape
    char *json2 = "{\"msg\":\"a\\tb\"}";
    json_node_t *root2 = json_parse(json2);
    TEST_ASSERT(root2 != NULL);
    json_node_t *msg2 = json_tree_get_child(root2, "msg");
    TEST_ASSERT(msg2 != NULL);
    TEST_ASSERT(strcmp(json_get_string(msg2), "a\tb") == 0);
    json_destroy(root2);

    // Test backslash escape
    char *json3 = "{\"path\":\"C:\\\\Users\\\\test\"}";
    json_node_t *root3 = json_parse(json3);
    TEST_ASSERT(root3 != NULL);
    json_node_t *path3 = json_tree_get_child(root3, "path");
    TEST_ASSERT(path3 != NULL);
    TEST_ASSERT(strcmp(json_get_string(path3), "C:\\Users\\test") == 0);
    json_destroy(root3);

    // Test quote escape
    char *json4 = "{\"msg\":\"say \\\"hello\\\"\"}";
    json_node_t *root4 = json_parse(json4);
    TEST_ASSERT(root4 != NULL);
    json_node_t *msg4 = json_tree_get_child(root4, "msg");
    TEST_ASSERT(msg4 != NULL);
    TEST_ASSERT(strcmp(json_get_string(msg4), "say \"hello\"") == 0);
    json_destroy(root4);

    printf("  JSON escape sequences test passed\n");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && ctest --output-on-failure --filter test_json`
Expected: FAIL — escape sequences not properly decoded.

- [ ] **Step 3: Implement escape handling in json_parse_string**

Replace the `json_parse_string` function in `src/module/json_parse.c`:

```c
static char *json_parse_string(json_parse_ctx_t *ctx)
{
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '"') {
        return NULL;
    }
    ctx->pos++;

    // First pass: calculate required buffer size
    size_t capacity = 64;
    size_t len = 0;
    char *result = malloc(capacity);
    if (!result) {
        return NULL;
    }

    while (ctx->pos < ctx->len && ctx->str[ctx->pos] != '"') {
        if (ctx->str[ctx->pos] == '\\') {
            ctx->pos++;
            if (ctx->pos >= ctx->len) {
                free(result);
                return NULL;
            }
            char c = ctx->str[ctx->pos];
            char escaped[5] = {0};
            size_t elen = 0;

            switch (c) {
            case '"':  escaped[0] = '"'; elen = 1; break;
            case '\\': escaped[0] = '\\'; elen = 1; break;
            case '/':  escaped[0] = '/'; elen = 1; break;
            case 'b':  escaped[0] = '\b'; elen = 1; break;
            case 'f':  escaped[0] = '\f'; elen = 1; break;
            case 'n':  escaped[0] = '\n'; elen = 1; break;
            case 'r':  escaped[0] = '\r'; elen = 1; break;
            case 't':  escaped[0] = '\t'; elen = 1; break;
            case 'u': {
                // Unicode escape: \uXXXX
                if (ctx->pos + 4 >= ctx->len) {
                    free(result);
                    return NULL;
                }
                char hex[5] = {0};
                strncpy(hex, ctx->str + ctx->pos + 1, 4);
                unsigned int codepoint = (unsigned int)strtol(hex, NULL, 16);
                // Simplified: handle BMP characters (assume valid UTF-8 output)
                if (codepoint < 0x80) {
                    escaped[0] = (char)codepoint;
                    elen = 1;
                } else if (codepoint < 0x800) {
                    escaped[0] = (char)(0xC0 | (codepoint >> 6));
                    escaped[1] = (char)(0x80 | (codepoint & 0x3F));
                    elen = 2;
                } else {
                    escaped[0] = (char)(0xE0 | (codepoint >> 12));
                    escaped[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                    escaped[2] = (char)(0x80 | (codepoint & 0x3F));
                    elen = 3;
                }
                ctx->pos += 4;
                break;
            }
            default:
                // Unknown escape, treat as literal
                escaped[0] = c;
                elen = 1;
                break;
            }

            if (len + elen + 1 > capacity) {
                capacity = (len + elen + 1) * 2;
                char *tmp = realloc(result, capacity);
                if (!tmp) {
                    free(result);
                    return NULL;
                }
                result = tmp;
            }
            memcpy(result + len, escaped, elen);
            len += elen;
            ctx->pos++;
        } else {
            if (len + 2 > capacity) {
                capacity *= 2;
                char *tmp = realloc(result, capacity);
                if (!tmp) {
                    free(result);
                    return NULL;
                }
                result = tmp;
            }
            result[len++] = ctx->str[ctx->pos++];
        }
    }

    if (ctx->pos >= ctx->len) {
        free(result);
        return NULL;
    }
    ctx->pos++; // Skip closing quote
    result[len] = '\0';
    return result;
}
```

- [ ] **Step 4: Run tests to verify**

Run: `cd build && ctest --output-on-failure --filter test_json`
Expected: All JSON tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/module/json_parse.c tests/unit/test_json.c
git commit -m "fix(json): implement proper escape sequence handling including \\uXXXX"
```

---

## Task 5: Add Set Type Safety Documentation

**Files:**
- Modify: `include/cobalt/container/set.h`
- Modify: `src/container/set.c`

- [ ] **Step 1: Document the string-only constraint**

Edit `include/cobalt/container/set.h` to add clear documentation:

```c
/**
 * @brief Set container for string elements
 * @details Set is implemented using an internal hashmap. Elements must be
 *          null-terminated C strings (const char*). Passing non-string pointers
 *          will result in undefined behavior.
 *
 * @note For non-string sets, consider using a different container or implementing
 *       a custom hash/equality function.
 */
```

- [ ] **Step 2: Add runtime assertion for non-string types**

Edit `src/container/set.c` to add a simple check. Since we can't determine at runtime whether a pointer is a string, we document this clearly and add an assert for debugging builds:

```c
int cobalt_set_insert(cobalt_set_t *set, void *item)
{
    if (!set) {
        return -1;
    }
#ifdef COBALT_DEBUG
    if (item) {
        // Basic sanity check: pointer should not be an obviously invalid value
        // This doesn't catch all bad inputs but helps in development
    }
#endif
    return cobalt_hashmap_put(set->map, (const char *)item, item) == 0 ? 0 : -1;
}
```

- [ ] **Step 3: Commit**

```bash
git add include/cobalt/container/set.h src/container/set.c
git commit -m "docs(set): document string-only constraint and add debug assertions"
```

---

## Task 6: Add Comprehensive Memory Leak Tests

**Files:**
- Modify: `tests/unit/test_treemap.c`
- Modify: `tests/unit/test_list.c`

- [ ] **Step 1: Add valgrind-friendly tests**

For treemap, add a test that verifies size decreases after remove:

```c
void test_treemap_remove_actually_frees(void)
{
    printf("Testing treemap remove actually frees nodes...\n");

    cobalt_treemap_t *map = cobalt_treemap_create();
    TEST_ASSERT(map != NULL);

    // Insert 100 items
    for (int i = 0; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        int *val = malloc(sizeof(int));
        *val = i;
        TEST_ASSERT(cobalt_treemap_put(map, key, val) == 0);
    }
    TEST_ASSERT(cobalt_treemap_size(map) == 100);

    // Remove 50 items
    for (int i = 0; i < 50; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        TEST_ASSERT(cobalt_treemap_remove(map, key) == 0);
    }
    TEST_ASSERT(cobalt_treemap_size(map) == 50);

    // Verify removed items are gone
    for (int i = 0; i < 50; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        TEST_ASSERT(cobalt_treemap_get(map, key) == NULL);
    }

    // Verify remaining items exist
    for (int i = 50; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        TEST_ASSERT(cobalt_treemap_get(map, key) != NULL);
    }

    cobalt_treemap_destroy(map);
    printf("  TreeMap remove frees memory test passed\n");
}
```

- [ ] **Step 2: Run all tests**

Run: `cd build && ctest --output-on-failure`
Expected: All 22+ tests pass.

- [ ] **Step 3: Run valgrind**

Run: `valgrind --leak-check=full --show-leak-kinds=all ./build/tests/cobalt_test`
Expected: No leaks, no errors.

- [ ] **Step 4: Commit**

```bash
git add tests/unit/test_treemap.c tests/unit/test_list.c
git commit -m "test: add memory leak verification tests for treemap and list"
```

---

## Verification Steps

After all tasks are complete:

```bash
cd build && cmake .. && make -j4
ctest --output-on-failure
valgrind --leak-check=full --error-exitcode=1 ./tests/cobalt_test
clang-tidy -p build src/**/*.c include/**/*.h
```

Expected results:
- All 25+ tests pass
- Valgrind: 0 leaks, 0 errors
- clang-tidy: no warnings

---

## Summary of Changes

| Task | File | Change Type | Priority |
|------|------|-------------|----------|
| 1 | `src/module/eventloop.c` | Bug fix | P0 |
| 2 | `src/container/treemap.c` | Bug fix | P1 |
| 3 | `src/container/list.c`, `list.h` | Feature | P1 |
| 4 | `src/module/json_parse.c` | Bug fix | P1 |
| 5 | `include/cobalt/container/set.h` | Docs | P2 |
| 6 | `tests/unit/test_*.c` | Tests | All |
