# Pike VM Regular Expression Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a high-performance, pure C11, zero-dependency linear regular expression engine (Pike VM / Thompson NFA) in Cobalt Layer 3, providing zero-allocation matching, capture groups, backreferences, and complete immunity to ReDoS catastrophic backtracking.

**Architecture:** The regex engine translates patterns into linear bytecode instructions (`cobalt_regex_inst_t[]`) via a recursive-descent single-pass compiler. Matching executes via a Pike VM maintaining dual thread lists with generation-based deduplication, guaranteeing strict $O(N \times M)$ linear time complexity. For patterns with $M \le 64$ instructions, the VM operates entirely on stack memory with zero heap allocations. Submatch boundaries are exported via zero-copy `cobalt_regex_capture_t` slice arrays.

**Tech Stack:** C11 (`stdatomic.h`, `stdbool.h`, `_Alignas`), CMake 3.14+, CTest, Clang-Tidy, Clang-Format, AddressSanitizer/LeakSanitizer (ASan/LSan).

---

## File Structure Map

| File Path | Role | Responsibility |
|---|---|---|
| `include/cobalt/algorithm/regex.h` | Public Header | Declarations of `cobalt_regex_t`, flags, `cobalt_regex_capture_t`, compile, search, exact match, test, and lifecycle. |
| `include/cobalt/cobalt.h` | Umbrella Header | Include `cobalt/algorithm/regex.h`. |
| `src/algorithm/regex.c` | Implementation | Lexer, recursive-descent compiler, bytecode generation, Pike VM execution engine with `OP_BACKREF` support. |
| `tests/unit/test_regex.c` | Unit Test | Test basic syntax, quantifiers, classes, groups, backreferences, flags, error handling, and custom allocator. |
| `tests/benchmark/benchmark_regex.c` | Benchmark | ReDoS vulnerability immunity test ($O(NM)$ linearity), IPv4 extraction, Email validation, and URL routing throughput. |
| `tests/benchmark/bench_runner.c` | Benchmark Runner | Integrate regex benchmarks into unified runner (text & JSON mode). |
| `tests/test_runner.c` | Test Runner | Register `test_regex` unit test suite. |
| `tests/CMakeLists.txt` | Build Config | Register `cobalt_test` unit tests and `benchmark_regex` CTest target. |
| `CMakeLists.txt` | Root Build Config | Add `src/algorithm/regex.c` to `COBALT_SOURCES`. |

---

## Tasks

### Task 1: Public Header Declarations & Build Plumbing

**Files:**
- Create: `include/cobalt/algorithm/regex.h`
- Create: `src/algorithm/regex.c` (stub)
- Modify: `include/cobalt/cobalt.h`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create `include/cobalt/algorithm/regex.h`**

```c
#ifndef COBALT_ALGORITHM_REGEX_H
#define COBALT_ALGORITHM_REGEX_H

/**
 * @file regex.h
 * @brief High-performance linear Pike VM regular expression engine
 * @details Implements a pure C11 Thompson NFA / Pike VM regular expression compiler
 *          and execution engine. Guarantees O(N * M) linear time matching with zero
 *          ReDoS catastrophic backtracking, stack-allocated fast path for zero heap
 *          allocations on small patterns, zero-copy capture slices, and backreference matching.
 *
 * @defgroup Regex Regular Expressions
 * @ingroup Algorithms
 * @{
 */

#include "cobalt/memory/allocator.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COBALT_REGEX_MAX_CAPTURES 16

typedef enum {
    COBALT_REGEX_DEFAULT          = 0,
    COBALT_REGEX_CASE_INSENSITIVE = 1 << 0,
    COBALT_REGEX_DOTALL           = 1 << 1,
    COBALT_REGEX_MULTILINE        = 1 << 2
} cobalt_regex_flags_t;

typedef struct {
    size_t start;
    size_t end;
    size_t length;
} cobalt_regex_capture_t;

typedef struct cobalt_regex cobalt_regex_t;

cobalt_regex_t *cobalt_regex_compile(const char *pattern, int flags);
cobalt_regex_t *cobalt_regex_compile_with_alloc(const char         *pattern,
                                                int                 flags,
                                                cobalt_allocator_t *alloc);
void cobalt_regex_destroy(cobalt_regex_t *re);

int cobalt_regex_search(const cobalt_regex_t   *re,
                        const char             *text,
                        cobalt_regex_capture_t *captures,
                        size_t                  max_captures);

int cobalt_regex_match_exact(const cobalt_regex_t   *re,
                             const char             *text,
                             cobalt_regex_capture_t *captures,
                             size_t                  max_captures);

int cobalt_regex_test(const char *pattern, const char *text);

size_t cobalt_regex_capture_count(const cobalt_regex_t *re);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* COBALT_ALGORITHM_REGEX_H */
```

- [ ] **Step 2: Create stub in `src/algorithm/regex.c`**

```c
#include "cobalt/algorithm/regex.h"
#include <stdlib.h>

struct cobalt_regex {
    int flags;
};

cobalt_regex_t *cobalt_regex_compile(const char *pattern, int flags)
{
    (void)pattern;
    (void)flags;
    return NULL;
}

cobalt_regex_t *cobalt_regex_compile_with_alloc(const char *pattern, int flags, cobalt_allocator_t *alloc)
{
    (void)pattern;
    (void)flags;
    (void)alloc;
    return NULL;
}

void cobalt_regex_destroy(cobalt_regex_t *re)
{
    (void)re;
}

int cobalt_regex_search(const cobalt_regex_t *re, const char *text, cobalt_regex_capture_t *captures, size_t max_captures)
{
    (void)re;
    (void)text;
    (void)captures;
    (void)max_captures;
    return -1;
}

int cobalt_regex_match_exact(const cobalt_regex_t *re, const char *text, cobalt_regex_capture_t *captures, size_t max_captures)
{
    (void)re;
    (void)text;
    (void)captures;
    (void)max_captures;
    return -1;
}

int cobalt_regex_test(const char *pattern, const char *text)
{
    (void)pattern;
    (void)text;
    return -1;
}

size_t cobalt_regex_capture_count(const cobalt_regex_t *re)
{
    (void)re;
    return 0;
}
```

- [ ] **Step 3: Update `include/cobalt/cobalt.h` and `CMakeLists.txt`**

Add `#include "cobalt/algorithm/regex.h"` to `cobalt.h`.
Add `src/algorithm/regex.c` to `COBALT_SOURCES` in `CMakeLists.txt`.

- [ ] **Step 4: Verify build and commit**

```bash
cmake --build build -j4
git add include/cobalt/algorithm/regex.h include/cobalt/cobalt.h src/algorithm/regex.c CMakeLists.txt
git commit -m "feat(regex): ✨ declare regex public APIs and setup build plumbing"
```

---

### Task 2: Bytecode Compiler (Lexer, Parser & Instruction Generator)

**Files:**
- Modify: `src/algorithm/regex.c`
- Create: `tests/unit/test_regex.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_runner.c`

- [ ] **Step 1: Write failing unit tests for regex compilation and syntax validation**

In `tests/unit/test_regex.c`:
- Test successful compilation of valid patterns:
  - Literal `abc`
  - Alternations `a|b|c`
  - Character classes `[a-z0-9_]`, `[^a-z]`, `\d`, `\w`, `\s`
  - Quantifiers `a*`, `a+`, `a?`, `a{2,5}`
  - Non-greedy `a*?`, `a+?`, `a??`
  - Groups `(abc)`, `(?:abc)`, and backreference `\1`
- Test compilation error detection returning NULL and setting `COBALT_ERROR_INVALID_ARGUMENT`:
  - Unmatched parenthesis: `(abc`
  - Unclosed bracket: `[a-z`
  - Invalid range: `[z-a]`
  - Invalid quantifier bounds: `{5,2}` or `{150}` ($> 128$)
  - Dangling quantifier: `*abc`, `+abc`, `?abc`
  - Undefined backreference: `\1` without capture group
  - Trailing escape: `abc\`

- [ ] **Step 2: Implement compiler in `src/algorithm/regex.c`**

1. Define opcode types:
   ```c
   typedef enum {
       OP_CHAR,
       OP_ANY,
       OP_CLASS,
       OP_BOL,
       OP_EOL,
       OP_WORD_BOUNDARY,
       OP_JUMP,
       OP_SPLIT,
       OP_SAVE,
       OP_BACKREF,
       OP_MATCH
   } cobalt_opcode_t;
   ```
2. Dynamic instruction vector with custom allocator support (`cobalt_regex_inst_t`).
3. 256-bit character class bitmap builder supporting ASCII character ranges and escapes (`\d`, `\w`, `\s`, `\D`, `\W`, `\S`).
4. Recursive-descent grammar:
   - `parse_expr`: handle alternation `|` with `OP_SPLIT` and `OP_JUMP`.
   - `parse_term`: handle concatenation sequence.
   - `parse_factor`: handle quantifiers `*`, `+`, `?`, `{m,n}`, and `?` non-greedy suffix.
   - `parse_atom`: handle literals, `.` (`OP_ANY`), `^` (`OP_BOL`), `$` (`OP_EOL`), `\b`/`\B`, `[...]`, `(expr)` with `OP_SAVE 2*k`/`OP_SAVE 2*k+1`, `(?:expr)`, and `\1` (`OP_BACKREF`).
5. Syntax error handling setting `COBALT_ERROR_INVALID_ARGUMENT`.

- [ ] **Step 3: Build and verify compilation tests pass**

```bash
cmake --build build -j4 && ./build/tests/cobalt_test --filter regex
```

- [ ] **Step 4: Commit**

```bash
git add src/algorithm/regex.c tests/unit/test_regex.c tests/CMakeLists.txt tests/test_runner.c
git commit -m "feat(regex): ✨ implement recursive-descent regex compiler and bytecode generator"
```

---

### Task 3: Pike VM Execution Core & Linear Matcher

**Files:**
- Modify: `src/algorithm/regex.c`
- Modify: `tests/unit/test_regex.c`

- [ ] **Step 1: Write failing unit tests for pattern matching**

In `tests/unit/test_regex.c`:
- `test_regex_literals`: exact string matching, partial string search.
- `test_regex_wildcard`: `.` matching any char, `COBALT_REGEX_DOTALL` matching newline.
- `test_regex_anchors`: `^` matching start of line, `$` matching end of line, `\b` matching word boundary.
- `test_regex_classes`: `[a-z]`, `[^0-9]`, `\d+`, `\w+`, `\s+`.
- `test_regex_quantifiers`: greedy vs non-greedy (`*` vs `*?`, `+` vs `+?`).
- `test_regex_alternation`: `cat|dog|fox`.
- `test_regex_convenience`: `cobalt_regex_test("hello.*world", "hello beautiful world") == 1`.

- [ ] **Step 2: Implement Pike VM in `src/algorithm/regex.c`**

1. Define `pike_thread_t` and `pike_thread_list_t`:
   ```c
   typedef struct {
       uint32_t pc;
       size_t   slots[2 * COBALT_REGEX_MAX_CAPTURES];
   } pike_thread_t;
   ```
2. Stack fast-path: for $M \le 64$, use stack arrays for `threads` and `visited_gen`. For $M > 64$, allocate scratch buffer using `re->alloc`.
3. Generation-based deduplication: `visited_gen[pc] == current_step`.
4. $\epsilon$-closure resolution: recursively expand `OP_SPLIT`, `OP_JUMP`, `OP_SAVE`, `OP_BOL`, `OP_EOL`, `OP_WORD_BOUNDARY` without consuming characters.
5. Character step: evaluate `OP_CHAR` (with `COBALT_REGEX_CASE_INSENSITIVE`), `OP_ANY`, `OP_CLASS`.
6. Terminate upon `OP_MATCH` or input exhaustion.
7. Implement `cobalt_regex_search` and `cobalt_regex_match_exact`.

- [ ] **Step 3: Run unit tests and verify they pass**

```bash
cmake --build build -j4 && ./build/tests/cobalt_test --filter regex
```

- [ ] **Step 4: Commit**

```bash
git add src/algorithm/regex.c tests/unit/test_regex.c
git commit -m "feat(regex): ✨ implement Pike VM linear regex execution engine"
```

---

### Task 4: Submatch Capture Extraction & Backreference Matching (`OP_BACKREF`)

**Files:**
- Modify: `src/algorithm/regex.c`
- Modify: `tests/unit/test_regex.c`

- [ ] **Step 1: Write failing unit tests for captures & backreferences**

In `tests/unit/test_regex.c`:
- `test_regex_capture_groups`:
  - `(\d{4})-(\d{2})-(\d{2})` on `"2026-10-17"`: verify group 0 is `"2026-10-17"`, group 1 is `"2026"`, group 2 is `"10"`, group 3 is `"17"`.
  - Nested groups `((a)(b(c)))`: verify inner and outer offsets.
- `test_regex_backreferences`:
  - `([a-z]+)\s+\1` matching `"hello hello"`, failing on `"hello world"`.
  - HTML tag pattern `<([a-z]+)>.*?</\1>` matching `<div>content</div>`, failing on `<div>content</span>`.
- `test_regex_custom_allocator`: verify zero leaks when custom allocator is injected.

- [ ] **Step 2: Implement capture extraction & `OP_BACKREF`**

1. In Pike VM thread step:
   - Handle `OP_BACKREF slot`:
     - Fetch `start = thread.slots[2 * slot]` and `end = thread.slots[2 * slot + 1]`.
     - Compute length $L = end - start$.
     - Compare $L$ bytes from `text + start` with input at current position `pos`.
     - If matched, advance thread by $L$ bytes and push to `next_threads`.
2. In `cobalt_regex_search` / `match_exact`:
   - Copy winning thread's `slots` into caller's `captures[k]` array (`start = slots[2*k]`, `end = slots[2*k+1]`, `length = end - start`).
   - Unmatched capture groups receive `start = (size_t)-1, end = (size_t)-1, length = 0`.

- [ ] **Step 3: Run tests and verify they pass**

```bash
cmake --build build -j4 && ./build/tests/cobalt_test --filter regex
```

- [ ] **Step 4: Commit**

```bash
git add src/algorithm/regex.c tests/unit/test_regex.c
git commit -m "feat(regex): ✨ implement submatch capture extraction and backreference matching"
```

---

### Task 5: Dedicated ReDoS Immunity & Workload Benchmark Suite

**Files:**
- Create: `tests/benchmark/benchmark_regex.c`
- Modify: `tests/benchmark/bench_runner.c`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create `tests/benchmark/benchmark_regex.c`**

1. ReDoS Vulnerability Immunity Benchmark:
   - Pathological pattern `^(a+)+$` against `"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaX"` (30 'a's).
   - Assert execution time $< 1.0$ ms (proving strictly linear $O(NM)$ immunity).
2. Real-World Workload Throughput:
   - IPv4 address extraction (`\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}`) over 5MB simulated server logs.
   - Email format validation (`^[\w\.-]+@[\w\.-]+\.[a-zA-Z]{2,6}$`) over 100,000 iterations.
   - URL routing capture extraction (`/api/v1/users/(\d+)/posts/([a-z0-9_-]+)`) over 100,000 iterations.
3. Print throughput and end with `printf("Benchmark completed\n");`.

- [ ] **Step 2: Integrate into `tests/CMakeLists.txt` and `tests/benchmark/bench_runner.c`**

1. In `tests/CMakeLists.txt`:
   ```cmake
   add_executable(benchmark_regex benchmark/benchmark_regex.c)
   target_link_libraries(benchmark_regex cobalt pthread m)
   add_test(NAME bench_regex COMMAND benchmark_regex
            PASS_REGULAR_EXPRESSION "Benchmark completed")
   ```
2. In `tests/benchmark/bench_runner.c`:
   - Include `"cobalt/algorithm/regex.h"`.
   - Add benchmark functions `bench_regex_redos`, `bench_regex_email`, `bench_regex_ipv4`.
   - Wire into `main()` in both text mode and `--json` mode.

- [ ] **Step 3: Build, execute benchmarks, and verify execution**

```bash
cmake --build build -j4
./build/tests/benchmark_regex
./build/tests/bench_runner
ctest --test-dir build --output-on-failure -R bench_regex
```

- [ ] **Step 4: Commit**

```bash
git add tests/benchmark/benchmark_regex.c tests/benchmark/bench_runner.c tests/CMakeLists.txt
git commit -m "test(bench): ⚡ add benchmark suite and ReDoS stress test for regex engine"
```

---

### Task 6: Full Test Suite, Sanitizer Check & Format Gate

**Files:**
- Repository-wide verification across `src/algorithm/regex.c`, `include/cobalt/algorithm/regex.h`, `tests/unit/test_regex.c`, `tests/benchmark/benchmark_regex.c`, documentation files.

- [ ] **Step 1: Code Formatting**

```bash
clang-format -i src/algorithm/regex.c include/cobalt/algorithm/regex.h tests/unit/test_regex.c tests/benchmark/benchmark_regex.c tests/benchmark/bench_runner.c
clang-format --dry-run -Werror src/algorithm/regex.c include/cobalt/algorithm/regex.h tests/unit/test_regex.c tests/benchmark/benchmark_regex.c tests/benchmark/bench_runner.c
```
Verify 0 formatting errors.

- [ ] **Step 2: Clang-Tidy Static Analysis**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCOBALT_RUN_TIDY=ON
cmake --build build -j4
```
Verify 0 compiler warnings under `-Wall -Wextra -Werror` and 0 clang-tidy warnings.

- [ ] **Step 3: Full CTest Execution**

```bash
ctest --test-dir build --output-on-failure
```
Verify all 56/56 tests pass (100%).

- [ ] **Step 4: AddressSanitizer & LeakSanitizer Verification**

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCOBALT_ENABLE_ASAN=ON
cmake --build build-asan -j4
ctest --test-dir build-asan --output-on-failure
```
Verify 100% tests pass with 0 memory leaks and 0 memory errors under ASan/LSan.

- [ ] **Step 5: Documentation Update & Release Commit**

Update `README.md`, `CHANGELOG.md`, `docs/API/`, `docs/SPEC/`.
```bash
git add src/ include/ tests/ docs/ README.md CHANGELOG.md
git commit -m "chore(release): 🔖 finalize Pike VM regex engine for Cobalt v2.7.0"
```
