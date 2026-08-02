# Cobalt Phase 3: Feature Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand Cobalt framework with new containers (Set, Deque), algorithms (bsearch, find_if, for_each), and improve code organization by splitting json.c module.

**Architecture:** Three-phase incremental approach: Phase 3a (code organization), 3b (new containers), 3c (new algorithms). Each phase is independently testable.

**Tech Stack:** C11, CMake, CTest, clang-tidy, clang-format

---

## File Structure

### Files to Create
- `src/module/json_parse.c` — JSON parsing implementation
- `src/module/json_serialize.c` — JSON serialization implementation
- `include/cobalt/container/set.h` — Set container header
- `src/container/set.c` — Set container implementation
- `include/cobalt/container/deque.h` — Deque container header
- `src/container/deque.c` — Deque container implementation
- `tests/unit/test_set.c` — Set container tests
- `tests/unit/test_deque.c` — Deque container tests

### Files to Modify
- `src/module/json.c` — Keep only public API
- `CMakeLists.txt` — Add new source files
- `include/cobalt/cobalt.h` — Add set.h and deque.h includes
- `include/cobalt/algorithm/functional.h` — Add new algorithm declarations
- `src/algorithm/functional.c` — Add new algorithm implementations
- `tests/unit/test_functional.c` — Add algorithm tests
- `tests/CMakeLists.txt` — Add new test files

---

## Task 1: JSON Module Split - Create Parser Module

**Files:**
- Create: `src/module/json_parse.c`
- Modify: `src/module/json.c`

- [ ] **Step 1: Create json_parse.c with parsing functions**

```c
#include "cobalt/module/json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct
{
    const char* str;
    int pos;
    int len;
} json_parse_ctx_t;

static inline int is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void json_skip_whitespace(json_parse_ctx_t* ctx)
{
    while (ctx->pos < ctx->len && is_space((unsigned char)ctx->str[ctx->pos])) {
        ctx->pos++;
    }
}

static json_node_t* json_node_create(json_type_t type)
{
    json_node_t* node = malloc(sizeof(json_node_t));
    if (node) {
        node->type = type;
        memset(&node->value, 0, sizeof(node->value));
        node->next = NULL;
        node->key = NULL;
    }
    return node;
}

static char* json_parse_string(json_parse_ctx_t* ctx)
{
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '"')
        return NULL;
    ctx->pos++;

    int start = ctx->pos;
    while (ctx->pos < ctx->len && ctx->str[ctx->pos] != '"') {
        if (ctx->str[ctx->pos] == '\\' && ctx->pos + 1 < ctx->len) {
            ctx->pos += 2;
        } else {
            ctx->pos++;
        }
    }

    if (ctx->pos >= ctx->len)
        return NULL;

    int len = ctx->pos - start;
    char* result = malloc(len + 1);
    if (!result)
        return NULL;

    strncpy(result, ctx->str + start, len);
    result[len] = '\0';
    ctx->pos++;
    return result;
}

static json_node_t* json_parse_value(json_parse_ctx_t* ctx);

static json_node_t* json_parse_object(json_parse_ctx_t* ctx)
{
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '{')
        return NULL;
    ctx->pos++;

    json_node_t* root = json_node_create(JSON_OBJECT);
    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}') {
        ctx->pos++;
        return root;
    }

    while (1) {
        json_skip_whitespace(ctx);
        if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '"')
            break;

        char* key = json_parse_string(ctx);
        if (!key)
            break;

        json_skip_whitespace(ctx);
        if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != ':') {
            free(key);
            break;
        }
        ctx->pos++;

        json_node_t* value = json_parse_value(ctx);
        if (!value) {
            free(key);
            break;
        }

        json_node_t* kv = json_node_create(JSON_OBJECT);
        kv->key = key;
        kv->next = value;
        if (!root->next) {
            root->next = kv;
        } else {
            json_node_t* tail = root->next;
            while (tail->next)
                tail = tail->next;
            tail->next = kv;
        }

        json_skip_whitespace(ctx);
        if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ',') {
            ctx->pos++;
            continue;
        }
        if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}') {
            ctx->pos++;
        }
        break;
    }

    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == '}') {
        ctx->pos++;
    }
    return root;
}

static json_node_t* json_parse_array(json_parse_ctx_t* ctx)
{
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->str[ctx->pos] != '[')
        return NULL;
    ctx->pos++;

    json_node_t* root = json_node_create(JSON_ARRAY);
    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ']') {
        ctx->pos++;
        return root;
    }

    while (1) {
        json_node_t* elem = json_parse_value(ctx);
        if (!elem)
            break;

        elem->next = root->next;
        root->next = elem;

        json_skip_whitespace(ctx);
        if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ',') {
            ctx->pos++;
            continue;
        }
        break;
    }

    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->len && ctx->str[ctx->pos] == ']') {
        ctx->pos++;
    }
    return root;
}

static json_node_t* json_parse_value(json_parse_ctx_t* ctx)
{
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len)
        return NULL;

    char c = ctx->str[ctx->pos];

    if (c == '{')
        return json_parse_object(ctx);
    if (c == '[')
        return json_parse_array(ctx);
    if (c == '"') {
        char* s = json_parse_string(ctx);
        if (!s)
            return NULL;
        json_node_t* node = json_node_create(JSON_STRING);
        node->value.string = s;
        return node;
    }

    if (c == '-' || isdigit((unsigned char)c)) {
        int start = ctx->pos;
        while (ctx->pos < ctx->len &&
               (isdigit((unsigned char)ctx->str[ctx->pos]) || ctx->str[ctx->pos] == '.')) {
            ctx->pos++;
        }
        int num_len = ctx->pos - start;
        char* num_str = malloc(num_len + 1);
        if (!num_str)
            return NULL;
        strncpy(num_str, ctx->str + start, num_len);
        num_str[num_len] = '\0';
        double val = atof(num_str);
        free(num_str);

        json_node_t* node = json_node_create(JSON_NUMBER);
        node->value.number = val;
        return node;
    }

    if (strncmp(ctx->str + ctx->pos, "true", 4) == 0) {
        ctx->pos += 4;
        return json_node_create(JSON_TRUE);
    }
    if (strncmp(ctx->str + ctx->pos, "false", 5) == 0) {
        ctx->pos += 5;
        return json_node_create(JSON_FALSE);
    }
    if (strncmp(ctx->str + ctx->pos, "null", 4) == 0) {
        ctx->pos += 4;
        return json_node_create(JSON_NULL);
    }

    return NULL;
}

json_node_t* json_parse(const char* text)
{
    if (!text)
        return NULL;
    int len = strlen(text);
    if (len == 0)
        return NULL;

    json_parse_ctx_t ctx = {.str = text, .pos = 0, .len = len};
    json_skip_whitespace(&ctx);
    if (ctx.pos >= ctx.len)
        return NULL;

    return json_parse_value(&ctx);
}
```

- [ ] **Step 2: Update json.c to include parser module**

Replace `src/module/json.c` with:

```c
#include "cobalt/module/json.h"
#include "json_parse.c"
#include "json_serialize.c"

/* Public API */
double json_get_number(json_node_t* node)
{
    if (node && node->type == JSON_NUMBER)
        return node->value.number;
    return 0.0;
}

const char* json_get_string(json_node_t* node)
{
    if (node && node->type == JSON_STRING && node->value.string)
        return node->value.string;
    return "";
}

int json_is_null(json_node_t* node)
{
    return node && node->type == JSON_NULL;
}

int json_is_object(json_node_t* node)
{
    return node && node->type == JSON_OBJECT;
}

int json_is_array(json_node_t* node)
{
    return node && node->type == JSON_ARRAY;
}

json_node_t* json_tree_get_child(json_node_t* parent, const char* key)
{
    if (!parent || !key || parent->type != JSON_OBJECT)
        return NULL;

    json_node_t* kv = parent->next;
    while (kv) {
        if (kv->key && strcmp(kv->key, key) == 0)
            return kv->next;
        kv = kv->next ? kv->next->next : NULL;
    }
    return NULL;
}
```

- [ ] **Step 3: Create json_serialize.c with serialization functions**

```c
#include "cobalt/module/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* json_escape_string(const char* str, size_t len)
{
    size_t out_len = 0;
    const char* p = str;
    const char* end = str + len;

    while (p < end) {
        char c = *p++;
        switch (c) {
        case '"':
            out_len += 2;
            break;
        case '\\':
            out_len += 2;
            break;
        case '\n':
            out_len += 2;
            break;
        case '\r':
            out_len += 2;
            break;
        case '\t':
            out_len += 2;
            break;
        default:
            if ((unsigned char)c < 0x20) {
                out_len += 6;
            } else {
                out_len += 1;
            }
            break;
        }
    }

    char* result = malloc(out_len + 1);
    if (!result)
        return NULL;

    p = str;
    char* out = result;
    while (p < end) {
        char c = *p++;
        switch (c) {
        case '"':
            *out++ = '\\';
            *out++ = '"';
            break;
        case '\\':
            *out++ = '\\';
            *out++ = '\\';
            break;
        case '\n':
            *out++ = '\\';
            *out++ = 'n';
            break;
        case '\r':
            *out++ = '\\';
            *out++ = 'r';
            break;
        case '\t':
            *out++ = '\\';
            *out++ = 't';
            break;
        default:
            if ((unsigned char)c < 0x20) {
                out += sprintf(out, "\\u00%02x", (unsigned char)c);
            } else {
                *out++ = c;
            }
            break;
        }
    }
    *out = '\0';
    return result;
}

char* json_serialize(json_node_t* node)
{
    if (!node)
        return cobalt_strdup("null");

    size_t capacity = 256;
    size_t length = 0;
    char* buffer = malloc(capacity);
    if (!buffer)
        return cobalt_strdup("{}");

    switch (node->type) {
    case JSON_NULL:
        if (length + 5 > capacity) {
            capacity = 512;
            char* tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        memcpy(buffer + length, "null", 5);
        length += 4;
        break;
    case JSON_TRUE:
        if (length + 5 > capacity) {
            capacity = 512;
            char* tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        memcpy(buffer + length, "true", 5);
        length += 4;
        break;
    case JSON_FALSE:
        if (length + 6 > capacity) {
            capacity = 512;
            char* tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        memcpy(buffer + length, "false", 6);
        length += 5;
        break;
    case JSON_NUMBER:
        length += snprintf(buffer + length, capacity - length, "%.17g", node->value.number);
        if (length >= capacity) {
            capacity = length + 64;
            char* tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
            length += snprintf(buffer + length, capacity - length, "%.17g", node->value.number);
        }
        break;
    case JSON_STRING:
        if (node->value.string) {
            char* escaped = json_escape_string(node->value.string, strlen(node->value.string));
            if (escaped) {
                length += snprintf(buffer + length, capacity - length, "\"%s\"", escaped);
                if (length >= capacity) {
                    capacity = length + 128;
                    char* tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(escaped);
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                    length += snprintf(buffer + length, capacity - length, "\"%s\"", escaped);
                }
                free(escaped);
            } else {
                if (length + 3 > capacity) {
                    capacity = 512;
                    char* tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                memcpy(buffer + length, "\"\"", 3);
                length += 2;
            }
        } else {
            if (length + 3 > capacity) {
                capacity = 512;
                char* tmp = realloc(buffer, capacity);
                if (!tmp) {
                    free(buffer);
                    return cobalt_strdup("{}");
                }
                buffer = tmp;
            }
            memcpy(buffer + length, "\"\"", 3);
            length += 2;
        }
        break;
    case JSON_ARRAY: {
        if (length + 2 > capacity) {
            capacity = 512;
            char* tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        memcpy(buffer + length, "[", 2);
        length += 1;
        json_node_t* child = node->next;
        int first = 1;
        while (child) {
            if (!first) {
                if (length + 2 > capacity) {
                    capacity *= 2;
                    char* tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                buffer[length++] = ',';
            }
            first = 0;
            char* s = json_serialize(child);
            if (s) {
                size_t slen = strlen(s);
                if (length + slen + 1 > capacity) {
                    capacity = length + slen + 64;
                    char* tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(s);
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                memcpy(buffer + length, s, slen);
                length += slen;
                free(s);
            }
            child = child->next;
        }
        if (length + 2 > capacity) {
            capacity = 512;
            char* tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        buffer[length++] = ']';
        break;
    }
    case JSON_OBJECT: {
        if (length + 2 > capacity) {
            capacity = 512;
            char* tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        memcpy(buffer + length, "{", 2);
        length += 1;
        json_node_t* kv = node->next;
        int first = 1;
        while (kv) {
            if (!first) {
                if (length + 2 > capacity) {
                    capacity *= 2;
                    char* tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                buffer[length++] = ',';
            }
            first = 0;
            char* key_escaped = json_escape_string(kv->key ? kv->key : "", 
                                                   strlen(kv->key ? kv->key : ""));
            if (key_escaped) {
                size_t klen = strlen(key_escaped);
                if (length + klen + 3 > capacity) {
                    capacity = length + klen + 128;
                    char* tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(key_escaped);
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                length += snprintf(buffer + length, capacity - length, "\"%s\":", key_escaped);
                free(key_escaped);
            }
            char* val = json_serialize(kv->next);
            if (val) {
                size_t vlen = strlen(val);
                if (length + vlen + 1 > capacity) {
                    capacity = length + vlen + 64;
                    char* tmp = realloc(buffer, capacity);
                    if (!tmp) {
                        free(val);
                        free(buffer);
                        return cobalt_strdup("{}");
                    }
                    buffer = tmp;
                }
                memcpy(buffer + length, val, vlen);
                length += vlen;
                free(val);
            }
            kv = kv->next ? kv->next->next : NULL;
        }
        if (length + 2 > capacity) {
            capacity = 512;
            char* tmp = realloc(buffer, capacity);
            if (!tmp) {
                free(buffer);
                return cobalt_strdup("{}");
            }
            buffer = tmp;
        }
        buffer[length++] = '}';
        break;
    }
    }

    buffer[length] = '\0';
    return buffer;
}
```

- [ ] **Step 4: Update CMakeLists.txt**

Add to SOURCES list:
```cmake
    src/module/json_parse.c
    src/module/json_serialize.c
```

Remove `src/module/json.c` from SOURCES.

- [ ] **Step 5: Build and run tests**

```bash
cd build && cmake .. && cmake --build . --parallel && ctest --output-on-failure
```

Expected: All 20 tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/module/json_parse.c src/module/json_serialize.c CMakeLists.txt
git commit -m "refactor: split json module into parser and serializer"
```

---

## Task 2: Implement Set Container

**Files:**
- Create: `include/cobalt/container/set.h`
- Create: `src/container/set.c`
- Create: `tests/unit/test_set.c`
- Modify: `CMakeLists.txt`
- Modify: `include/cobalt/cobalt.h`

- [ ] **Step 1: Create set.h header**

```c
#ifndef SET_H
#define SET_H

#include <stddef.h>

typedef struct cobalt_set cobalt_set_t;

cobalt_set_t* cobalt_set_create(size_t initial_capacity);
void cobalt_set_destroy(cobalt_set_t* set);
int cobalt_set_insert(cobalt_set_t* set, void* item);
int cobalt_set_remove(cobalt_set_t* set, void* item);
int cobalt_set_contains(cobalt_set_t* set, void* item);
size_t cobalt_set_size(cobalt_set_t* set);
int cobalt_set_is_empty(cobalt_set_t* set);

#endif /* SET_H */
```

- [ ] **Step 2: Create set.c implementation**

```c
#include "cobalt/container/set.h"
#include "cobalt/container/hashmap.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

struct cobalt_set
{
    cobalt_hashmap_t* map;
};

cobalt_set_t* cobalt_set_create(size_t initial_capacity)
{
    cobalt_set_t* set = malloc(sizeof(cobalt_set_t));
    if (!set)
        return NULL;

    set->map = cobalt_hashmap_create(initial_capacity);
    if (!set->map) {
        free(set);
        return NULL;
    }

    return set;
}

void cobalt_set_destroy(cobalt_set_t* set)
{
    if (set) {
        cobalt_hashmap_destroy(set->map);
        free(set);
    }
}

int cobalt_set_insert(cobalt_set_t* set, void* item)
{
    if (!set)
        return -1;
    return cobalt_hashmap_put(set->map, (const char*)item, item) == 0 ? 0 : -1;
}

int cobalt_set_remove(cobalt_set_t* set, void* item)
{
    if (!set || !item)
        return -1;
    return cobalt_hashmap_remove(set->map, (const char*)item) == 0 ? 0 : -1;
}

int cobalt_set_contains(cobalt_set_t* set, void* item)
{
    if (!set || !item)
        return 0;
    return cobalt_hashmap_get(set->map, (const char*)item) != NULL;
}

size_t cobalt_set_size(cobalt_set_t* set)
{
    if (!set)
        return 0;
    return cobalt_hashmap_size(set->map);
}

int cobalt_set_is_empty(cobalt_set_t* set)
{
    return set ? cobalt_hashmap_size(set->map) == 0 : 1;
}
```

- [ ] **Step 3: Create test_set.c**

```c
#include "cobalt/container/set.h"
#include "test_framework.h"
#include <stdio.h>

void test_set_basic(void)
{
    printf("Testing set basic operations...\n");
    
    cobalt_set_t* set = cobalt_set_create(16);
    TEST_ASSERT(set != NULL);
    TEST_ASSERT(cobalt_set_is_empty(set));
    TEST_ASSERT(cobalt_set_size(set) == 0);
    
    int a = 1, b = 2, c = 3;
    
    TEST_ASSERT(cobalt_set_insert(set, &a) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 1);
    TEST_ASSERT(cobalt_set_contains(set, &a));
    printf("  Insert and contains: OK\n");
    
    TEST_ASSERT(cobalt_set_insert(set, &b) == 0);
    TEST_ASSERT(cobalt_set_insert(set, &c) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 3);
    printf("  Multiple inserts: OK\n");
    
    TEST_ASSERT(cobalt_set_contains(set, &a));
    TEST_ASSERT(cobalt_set_contains(set, &b));
    TEST_ASSERT(cobalt_set_contains(set, &c));
    TEST_ASSERT(!cobalt_set_contains(set, &a));
    printf("  Contains check: OK\n");
    
    TEST_ASSERT(cobalt_set_remove(set, &b) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 2);
    TEST_ASSERT(!cobalt_set_contains(set, &b));
    printf("  Remove: OK\n");
    
    TEST_ASSERT(cobalt_set_remove(set, &b) == -1);
    printf("  Remove non-existent: OK\n");
    
    cobalt_set_destroy(set);
    printf("  Set basic test passed\n");
}

void test_set_duplicates(void)
{
    printf("Testing set duplicate handling...\n");
    
    cobalt_set_t* set = cobalt_set_create(4);
    TEST_ASSERT(set != NULL);
    
    int val = 42;
    TEST_ASSERT(cobalt_set_insert(set, &val) == 0);
    TEST_ASSERT(cobalt_set_insert(set, &val) == 0);
    TEST_ASSERT(cobalt_set_size(set) == 1);
    
    cobalt_set_destroy(set);
    printf("  Duplicate insert (idempotent): OK\n");
}

void test_set_empty(void)
{
    printf("Testing set empty operations...\n");
    
    cobalt_set_t* set = cobalt_set_create(0);
    TEST_ASSERT(set != NULL);
    TEST_ASSERT(cobalt_set_is_empty(set));
    TEST_ASSERT(cobalt_set_size(set) == 0);
    
    TEST_ASSERT(cobalt_set_remove(set, NULL) == -1);
    TEST_ASSERT(cobalt_set_contains(set, NULL) == 0);
    
    cobalt_set_destroy(set);
    
    TEST_ASSERT(cobalt_set_contains(NULL, NULL) == 0);
    TEST_ASSERT(cobalt_set_size(NULL) == 0);
    cobalt_set_destroy(NULL);
    printf("  Empty and NULL safety: OK\n");
}

void test_set(void)
{
    printf("Testing set...\n");
    test_set_basic();
    test_set_duplicates();
    test_set_empty();
    printf("  Set tests completed\n");
}
```

- [ ] **Step 4: Update CMakeLists.txt and cobalt.h**

Add to SOURCES:
```cmake
    src/container/set.c
```

Add to tests/CMakeLists.txt:
```cmake
    unit/test_set.c
```

Add to `include/cobalt/cobalt.h`:
```c
#include <cobalt/container/set.h>
```

- [ ] **Step 5: Build and test**

```bash
cd build && cmake .. && cmake --build . --parallel && ctest --output-on-failure
```

Expected: 21 tests pass (20 original + 1 new set test).

- [ ] **Step 6: Commit**

```bash
git add include/cobalt/container/set.h src/container/set.c tests/unit/test_set.c
git commit -m "feat(set): add hash-based set container"
```

---

## Task 3: Implement Deque Container

**Files:**
- Create: `include/cobalt/container/deque.h`
- Create: `src/container/deque.c`
- Create: `tests/unit/test_deque.c`
- Modify: `CMakeLists.txt`
- Modify: `include/cobalt/cobalt.h`

- [ ] **Step 1: Create deque.h header**

```c
#ifndef DEQUE_H
#define DEQUE_H

#include <stddef.h>

typedef struct cobalt_deque cobalt_deque_t;

cobalt_deque_t* cobalt_deque_create(void);
void cobalt_deque_destroy(cobalt_deque_t* deque);
int cobalt_deque_push_front(cobalt_deque_t* deque, void* item);
int cobalt_deque_push_back(cobalt_deque_t* deque, void* item);
void* cobalt_deque_pop_front(cobalt_deque_t* deque);
void* cobalt_deque_pop_back(cobalt_deque_t* deque);
void* cobalt_deque_peek_front(cobalt_deque_t* deque);
void* cobalt_deque_peek_back(cobalt_deque_t* deque);
size_t cobalt_deque_size(cobalt_deque_t* deque);
int cobalt_deque_is_empty(cobalt_deque_t* deque);

#endif /* DEQUE_H */
```

- [ ] **Step 2: Create deque.c implementation**

```c
#include "cobalt/container/deque.h"
#include "cobalt/runtime/error.h"
#include <stdlib.h>

typedef struct deque_node
{
    void* data;
    struct deque_node* next;
    struct deque_node* prev;
} deque_node_t;

struct cobalt_deque
{
    deque_node_t* head;
    deque_node_t* tail;
    size_t size;
};

cobalt_deque_t* cobalt_deque_create(void)
{
    cobalt_deque_t* deque = malloc(sizeof(cobalt_deque_t));
    if (!deque)
        return NULL;
    deque->head = NULL;
    deque->tail = NULL;
    deque->size = 0;
    return deque;
}

void cobalt_deque_destroy(cobalt_deque_t* deque)
{
    if (!deque)
        return;
    
    deque_node_t* node = deque->head;
    while (node) {
        deque_node_t* next = node->next;
        free(node);
        node = next;
    }
    free(deque);
}

int cobalt_deque_push_front(cobalt_deque_t* deque, void* item)
{
    if (!deque)
        return -1;
    
    deque_node_t* node = malloc(sizeof(deque_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    
    node->data = item;
    node->prev = NULL;
    node->next = deque->head;
    
    if (deque->head)
        deque->head->prev = node;
    else
        deque->tail = node;
    
    deque->head = node;
    deque->size++;
    return 0;
}

int cobalt_deque_push_back(cobalt_deque_t* deque, void* item)
{
    if (!deque)
        return -1;
    
    deque_node_t* node = malloc(sizeof(deque_node_t));
    if (!node) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    
    node->data = item;
    node->next = NULL;
    node->prev = deque->tail;
    
    if (deque->tail)
        deque->tail->next = node;
    else
        deque->head = node;
    
    deque->tail = node;
    deque->size++;
    return 0;
}

void* cobalt_deque_pop_front(cobalt_deque_t* deque)
{
    if (!deque || !deque->head)
        return NULL;
    
    deque_node_t* node = deque->head;
    void* data = node->data;
    
    deque->head = node->next;
    if (deque->head)
        deque->head->prev = NULL;
    else
        deque->tail = NULL;
    
    free(node);
    deque->size--;
    return data;
}

void* cobalt_deque_pop_back(cobalt_deque_t* deque)
{
    if (!deque || !deque->tail)
        return NULL;
    
    deque_node_t* node = deque->tail;
    void* data = node->data;
    
    deque->tail = node->prev;
    if (deque->tail)
        deque->tail->next = NULL;
    else
        deque->head = NULL;
    
    free(node);
    deque->size--;
    return data;
}

void* cobalt_deque_peek_front(cobalt_deque_t* deque)
{
    if (!deque || !deque->head)
        return NULL;
    return deque->head->data;
}

void* cobalt_deque_peek_back(cobalt_deque_t* deque)
{
    if (!deque || !deque->tail)
        return NULL;
    return deque->tail->data;
}

size_t cobalt_deque_size(cobalt_deque_t* deque)
{
    return deque ? deque->size : 0;
}

int cobalt_deque_is_empty(cobalt_deque_t* deque)
{
    return deque ? deque->size == 0 : 1;
}
```

- [ ] **Step 3: Create test_deque.c**

```c
#include "cobalt/container/deque.h"
#include "test_framework.h"
#include <stdio.h>

void test deque basic(void)
{
    printf("Testing deque basic operations...\n");
    
    cobalt_deque_t* dq = cobalt_deque_create();
    TEST_ASSERT(dq != NULL);
    TEST_ASSERT(cobalt_deque_is_empty(dq));
    TEST_ASSERT(cobalt_deque_size(dq) == 0);
    
    int a = 1, b = 2, c = 3;
    
    TEST_ASSERT(cobalt_deque_push_back(dq, &a) == 0);
    TEST_ASSERT(cobalt_deque_push_back(dq, &b) == 0);
    TEST_ASSERT(cobalt_deque_push_back(dq, &c) == 0);
    TEST_ASSERT(cobalt_deque_size(dq) == 3);
    printf("  Push back: OK\n");
    
    TEST_ASSERT(cobalt_deque_push_front(dq, &a) == 0);
    TEST_ASSERT(cobalt_deque_size(dq) == 4);
    printf("  Push front: OK\n");
    
    void* item = cobalt_deque_pop_front(dq);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int*)item == 1);
    printf("  Pop front: OK\n");
    
    item = cobalt_deque_pop_back(dq);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int*)item == 3);
    printf("  Pop back: OK\n");
    
    TEST_ASSERT(cobalt_deque_size(dq) == 2);
    
    cobalt_deque_destroy(dq);
    printf("  Deque basic test passed\n");
}

void test deque peek(void)
{
    printf("Testing deque peek operations...\n");
    
    cobalt_deque_t* dq = cobalt_deque_create();
    TEST_ASSERT(dq != NULL);
    
    int a = 10, b = 20;
    cobalt_deque_push_back(dq, &a);
    cobalt_deque_push_back(dq, &b);
    
    void* front = cobalt_deque_peek_front(dq);
    TEST_ASSERT(front != NULL);
    TEST_ASSERT(*(int*)front == 10);
    
    void* back = cobalt_deque_peek_back(dq);
    TEST_ASSERT(back != NULL);
    TEST_ASSERT(*(int*)back == 20);
    
    TEST_ASSERT(cobalt_deque_size(dq) == 2);
    
    cobalt_deque_destroy(dq);
    printf("  Peek operations: OK\n");
}

void test deque empty(void)
{
    printf("Testing deque empty operations...\n");
    
    cobalt_deque_t* dq = cobalt_deque_create();
    TEST_ASSERT(dq != NULL);
    
    TEST_ASSERT(cobalt_deque_pop_front(dq) == NULL);
    TEST_ASSERT(cobalt_deque_pop_back(dq) == NULL);
    TEST_ASSERT(cobalt_deque_peek_front(dq) == NULL);
    TEST_ASSERT(cobalt_deque_peek_back(dq) == NULL);
    
    cobalt_deque_destroy(dq);
    
    TEST_ASSERT(cobalt_deque_pop_front(NULL) == NULL);
    TEST_ASSERT(cobalt_deque_pop_back(NULL) == NULL);
    cobalt_deque_destroy(NULL);
    printf("  Empty and NULL safety: OK\n");
}

void test deque(void)
{
    printf("Testing deque...\n");
    test deque basic();
    test deque peek();
    test deque empty();
    printf("  Deque tests completed\n");
}
```

- [ ] **Step 4: Update CMakeLists.txt and cobalt.h**

Add to SOURCES:
```cmake
    src/container/deque.c
```

Add to tests/CMakeLists.txt:
```cmake
    unit/test_deque.c
```

Add to `include/cobalt/cobalt.h`:
```c
#include <cobalt/container/deque.h>
```

- [ ] **Step 5: Build and test**

```bash
cd build && cmake .. && cmake --build . --parallel && ctest --output-on-failure
```

Expected: 22 tests pass (20 original + set + deque).

- [ ] **Step 6: Commit**

```bash
git add include/cobalt/container/deque.h src/container/deque.c tests/unit/test_deque.c
git commit -m "feat(deque): add double-ended queue container"
```

---

## Task 4: Add New Algorithms

**Files:**
- Modify: `include/cobalt/algorithm/functional.h`
- Modify: `src/algorithm/functional.c`
- Modify: `tests/unit/test_functional.c`

- [ ] **Step 1: Add algorithm declarations to functional.h**

Add after existing declarations:
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

- [ ] **Step 2: Implement algorithms in functional.c**

Add to `src/algorithm/functional.c`:
```c
void* cobalt_bsearch(const void* key, const void* base, size_t nmemb, 
                     size_t size, compare_func_t compar)
{
    if (!key || !base || nmemb == 0 || !compar)
        return NULL;
    
    const char* arr = (const char*)base;
    size_t left = 0;
    size_t right = nmemb;
    
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        int cmp = compar(key, arr + mid * size);
        
        if (cmp == 0)
            return (void*)(arr + mid * size);
        else if (cmp < 0)
            right = mid;
        else
            left = mid + 1;
    }
    
    return NULL;
}

void* cobalt_find_if(const void* base, size_t nmemb, size_t size,
                     predicate_func_t pred)
{
    if (!base || nmemb == 0 || !pred)
        return NULL;
    
    const char* arr = (const char*)base;
    for (size_t i = 0; i < nmemb; i++) {
        if (pred(arr + i * size))
            return (void*)(arr + i * size);
    }
    
    return NULL;
}

void cobalt_for_each(const void* base, size_t nmemb, size_t size,
                     operation_func_t op)
{
    if (!base || nmemb == 0 || !op)
        return;
    
    const char* arr = (const char*)base;
    for (size_t i = 0; i < nmemb; i++) {
        op((void*)(arr + i * size));
    }
}
```

- [ ] **Step 3: Add tests to test_functional.c**

Add new test functions:
```c
void test_functional_bsearch(void)
{
    printf("Testing cobalt_bsearch...\n");
    
    int values[] = {1, 3, 5, 7, 9, 11, 13, 15};
    int n = sizeof(values) / sizeof(values[0]);
    
    int key = 7;
    int* result = (int*)cobalt_bsearch(&key, values, n, sizeof(int), 
                                       (compare_func_t)compar_int);
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(*result == 7);
    printf("  Find existing element: OK\n");
    
    key = 10;
    result = (int*)cobalt_bsearch(&key, values, n, sizeof(int),
                                  (compare_func_t)compar_int);
    TEST_ASSERT(result == NULL);
    printf("  Find non-existing element: OK\n");
    
    key = 1;
    result = (int*)cobalt_bsearch(&key, values, n, sizeof(int),
                                  (compare_func_t)compar_int);
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(*result == 1);
    printf("  Find first element: OK\n");
    
    key = 15;
    result = (int*)cobalt_bsearch(&key, values, n, sizeof(int),
                                  (compare_func_t)compar_int);
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(*result == 15);
    printf("  Find last element: OK\n");
}

void test_functional_find_if(void)
{
    printf("Testing cobalt_find_if...\n");
    
    int values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int n = sizeof(values) / sizeof(values[0]);
    
    int* result = (int*)cobalt_find_if(values, n, sizeof(int), predicate_even);
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(*result == 2);
    printf("  Find first even: OK\n");
    
    result = (int*)cobalt_find_if(values, n, sizeof(int), predicate_greater_than_5);
    TEST_ASSERT(result != NULL);
    TEST_ASSERT(*result == 6);
    printf("  Find element > 5: OK\n");
    
    result = (int*)cobalt_find_if(values, n, sizeof(int), predicate_greater_than_100);
    TEST_ASSERT(result == NULL);
    printf("  Find non-existing: OK\n");
}

void test_functional_for_each(void)
{
    printf("Testing cobalt_for_each...\n");
    
    int values[] = {1, 2, 3, 4, 5};
    int n = sizeof(values) / sizeof(values[0]);
    int sum = 0;
    
    cobalt_for_each(values, n, sizeof(int), sum_adder);
    TEST_ASSERT(sum == 15);
    printf("  Sum all elements: OK\n");
    
    sum = 0;
    cobalt_for_each(values, n, sizeof(int), sum_negator);
    TEST_ASSERT(sum == -15);
    printf("  Negate all elements: OK\n");
}
```

- [ ] **Step 4: Build and test**

```bash
cd build && cmake .. && cmake --build . --parallel && ctest --output-on-failure
```

Expected: All 22 tests pass.

- [ ] **Step 5: Commit**

```bash
git add include/cobalt/algorithm/functional.h src/algorithm/functional.c
git commit -m "feat(algorithm): add bsearch, find_if, and for_each"
```

---

## Verification

After completing all tasks:

1. Run full test suite:
   ```bash
   cd build && ctest --output-on-failure
   ```
   Expected: 22/22 tests pass

2. Run clang-tidy:
   ```bash
   cmake --build build --target tidy
   ```

3. Run clang-format check:
   ```bash
   cmake --build build --target format
   ```

4. Verify examples still build and run:
   ```bash
   for exe in examples_*; do
       echo "Running $exe..."
       ./build/$exe
   done
   ```
