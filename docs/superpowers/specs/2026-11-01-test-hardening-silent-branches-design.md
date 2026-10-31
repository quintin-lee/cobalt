# Cobalt Test Hardening — Silent Branch Elimination — Specification

**Date:** 2026-11-01  
**Status:** Draft  
**Scope:** 13 unit test files with `if/printf` silent-pass branches  
**Authors:** Antigravity & Cobalt Maintainer

---

## 1. Overview & Goals

Several unit test files in `tests/unit/` still use the pattern

```c
if (cond) {
    printf("  ...: OK\n");
} else {
    fprintf(stderr, "ERROR: ...\n");
}
```

which prints an error but never exits non-zero, so `ctest` reports green even when the assertion has failed. These branches are "silent-pass": the test lies.

**Goal:** Replace every such silent-pass branch with `TEST_ASSERT(cond)` / `TEST_EQUAL(actual, expected)` from `tests/test_framework.h`, which aborts the process on failure, restoring genuine failure detection.

**Non-goals:**
- Do not change product code (`src/`).
- Do not add new test cases beyond converting existing silent-pass branches.
- Do not restructure test functions beyond the minimal `if` → `TEST_ASSERT` conversion.

---

## 2. Architecture

All changes live in `tests/unit/`. One commit per test file; each commit is self-contained and verified with `ctest -R test_<name>`.

### Component A — Conversion rules (applied per file)

| Before | After |
|---|---|
| `if (ptr != NULL) { printf OK } else { fprintf ERROR }` | `TEST_ASSERT(ptr != NULL)` |
| `if (ret == 0) { printf OK } else { fprintf ERROR }` | `TEST_EQUAL(ret, 0)` |
| `if (val && *val == N) { printf OK }` | `TEST_ASSERT(val != NULL); TEST_EQUAL(*val, N)` |
| `if (cond) { printf OK; cleanup(); } else { fprintf ERROR; cleanup(); }` | `TEST_ASSERT(cond); cleanup();` (cleanup moved before the assert; on failure the process exits and the OS reclaims resources) |
| `if (a && b) { printf OK }` | `TEST_ASSERT(a); TEST_ASSERT(b);` |

Progress `printf` calls that are not gated by an `if` (e.g. `"Testing X..."`) are left untouched.

### Component B — Per-file verification

After converting each file:

1. `cmake --build build --parallel` (or the existing Release/Debug build dir)
2. `ctest -R test_<name> --output-on-failure`
3. Confirm 100% pass, then commit.

### Data flow

```
test_<name>.c  --(if/printf branches)-->  test_<name>.c  --(TEST_ASSERT/TEST_EQUAL)-->  ctest -R test_<name>  -->  real failure detection
```

---

## 3. Error Handling

- `TEST_ASSERT`/`TEST_EQUAL` call `exit(1)` on failure, so a failed assertion propagates to `ctest` as a non-zero exit and the job fails. No silent pass remains.
- Cleanup (`cobalt_*_destroy`, `free`, `fclose`) that was inside the `else`/success branches is moved before the assertion or dropped where the process exit makes it unnecessary. Memory leaks in a failing test process are acceptable (process termination reclaims them).
- If an existing `if` branch contains logic beyond `printf`/`fprintf` (e.g. counter increments), that logic is preserved and only the print/fprintf is replaced by the assertion.

---

## 4. Testing Strategy

Per file `test_<name>.c`:

1. Convert all `if/printf` silent branches.
2. Build: `cmake --build build --parallel`.
3. Run: `ctest -R test_<name> --output-on-failure`.
4. Confirm `100% tests passed`.
5. Commit the file.

The full suite (`ctest --output-on-failure --timeout 120`) is run after all 13 files are done as final verification.

---

## 5. Scope Check

Target files (13, confirmed by grep):

1. `tests/unit/test_arena.c`
2. `tests/unit/test_class.c`
3. `tests/unit/test_error.c`
4. `tests/unit/test_eventloop.c`
5. `tests/unit/test_functional.c`
6. `tests/unit/test_hashmap.c`
7. `tests/unit/test_json.c`
8. `tests/unit/test_list.c`
9. `tests/unit/test_queue.c`
10. `tests/unit/test_sort.c`
11. `tests/unit/test_stack.c`
12. `tests/unit/test_treemap.c`
13. `tests/unit/test_vector.c`

This is focused enough for one implementation plan; no decomposition needed. Each file is independent of the others.

---

## 6. Ambiguity Check

- **"`if` with only a success branch (no `else`)":** converted to `TEST_ASSERT`; no ambiguity.
- **"`if` with `printf` in both branches":** converted to `TEST_ASSERT` for the success condition; the failure branch's `fprintf` is dropped (the assertion's own message covers it).
- **"`printf` inside `if` but also other statements":** other statements are preserved; only the print/fprintf → assertion conversion is applied.
- **Already-hardened files** (test_object, test_interface, test_iterator, test_atomic, test_logger, test_platform, and those fixed in earlier sessions) are not touched again.
