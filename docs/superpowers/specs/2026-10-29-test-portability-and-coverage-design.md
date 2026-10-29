# Cobalt — Test Portability Windows Fix & Public-API Coverage Backfill (Design)

**Date:** 2026-10-29
**Status:** Draft — awaiting user review
**Scope:** Test-only. No product code (`src/`, `include/`) changes.
**Context:** Immediately follows the P0+P1 test hardening commits (f6ba941…b1b13ba).

## 1. Problem

### 1.1 Windows CI is red (regression introduced by the hardening commits)

Run `40788005903` (push of f6ba941…b1b13ba) passed every job except `windows (Release)`
and `windows (Debug)`, which both fail at the **Test** step:

```
9/62 Test #9: test_logger ......................***Failed    0.01 sec
Assertion failed: log_fp != NULL at …\tests\unit\test_logger.c:20
```

`test_logger.c:19` calls `fopen("/tmp/cobalt_test_log.txt", "w")`. MinGW has no `/tmp`,
so `fopen` returns NULL. Before the hardening commits this line was
`fprintf(stderr, "WARNING: Cannot open temp log file\n"); return;` — the test silently
skipped itself and ctest stayed green. The new `TEST_ASSERT(log_fp != NULL)` is correct
by design: it surfaces the latent bug instead of hiding it.

So this is not a test-logic error; it is a latent portability bug that the hardening
work exposed. The fix belongs in the test, not in the assertion.

### 1.2 Public-API coverage gaps

A symbol-by-symbol audit of all `include/cobalt/**/*.h` against `tests/` found 31
public symbols with no test reference. That raw count overstates the problem: many
`*_create_with_allocator` variants are reached transitively because
`cobalt_X_create()` delegates to `cobalt_X_create_with_allocator(system)`, and
`tests/unit/test_allocator_inject.c` (450 LOC) already exercises the allocator-injection
mechanism for vector, hashmap, list, stack, queue, set, deque, pool, slab, class,
interface and iterator.

The real remaining risk is the **injection boundary**: NULL-allocator fallback
branches, allocator propagation into per-node allocations, and symbols with no
delegation chain at all.

## 2. Goals

1. Make `test_logger` portable so the `windows` jobs return to green.
2. Establish a portable temp-file convention in the test framework so this class of
   bug cannot recur silently.
3. Backfill coverage on the public symbols that are genuinely untested, prioritized by
   observable behavior rather than line count.

## 3. Non-Goals

- No changes to `src/` or `include/cobalt/` — product behavior stays fixed.
- No work on `cobalt_object_implements()` or `cobalt_platform_get_handle()`, both
  documented placeholders and previously confirmed out of scope.
- No compile-time coverage enforcement (rejected as premature — see §7).
- No `fork`/`waitpid` isolation to test `cobalt_fatal`'s `exit(1)`.

## 4. Part 1 — Windows Fix

### 4.1 Mechanism

Add to `tests/test_framework.h`:

```c
/* Unnamed, auto-deleted temp file, portable across POSIX and Windows (MinGW).
 * C89 tmpfile(): no path construction, no name collision, removed on close.
 * Returns NULL on failure. */
static FILE *cobalt_test_tmpfile(void)
{
    return tmpfile();
}
```

Why `tmpfile()` rather than `GetTempPath` + unique filename on Windows:

| Criterion | `tmpfile()` | `GetTempPath` + `mkstemp` analogue |
|---|---|---|
| Platform branches | none | `#ifdef _WIN32` + Win32 error handling |
| Name collision | impossible | must generate unique suffix |
| Cleanup on early return | automatic | manual `unlink` on every exit path |
| Available on MinGW | yes (C89 `<stdio.h>`) | needs `<windows.h>` include |

An `#ifdef`-free helper is worth more here than a "more realistic" path, because the
test only needs *some* writable, readable-back stream.

### 4.2 Constraint: rewrite instead of close-and-reopen

`tmpfile()` returns a handle that cannot be reopened, so the existing pattern

```c
FILE *fp = fopen(path, "w");  /* write */
fclose(fp);
fp = fopen(path, "r");        /* read back */
```

must become

```c
FILE *fp = cobalt_test_tmpfile();
fputs(..., fp);
fflush(fp);      /* flush user-space buffer before reading */
rewind(fp);      /* seek back to start */
fgets(...);      /* read back through the same handle */
fclose(fp);
```

Verified locally with `-Wall -Wextra -Werror` that `fflush` + `rewind` + `fgets`
recovers the expected per-severity line counts.

### 4.3 Affected sites

`tests/unit/test_logger.c` lines 19, 38, 102, 114, 156, 162 — six `fopen` calls across
two test functions (`test_logger_levels` lines 19/38; `test_logger_level_reset` lines
102/114/156/162). All six switch to the helper. `test_logger_null_stream` and
`test_logger_macros` write to `stdout` and need no change.

Assertion style: assert the return of `fflush`/`rewind` rather than a bare pointer
check, so a failure message names the actual I/O operation that failed.

## 5. Part 2 — Portability Convention

Full scan of `tests/` for hardcoded POSIX paths:

| Site | Path | Current guard | Disposition |
|---|---|---|---|
| `test_logger.c:19,38,102,114,156,162` | `/tmp/cobalt_test_log*.txt` | **none** | Fix in §4 |
| `test_shm.c:1649` | `/dev/shm/`, `/tmp/` | inside `#ifndef _WIN32` | Unchanged |
| `test_shm.c` (cleanup) | `/tmp/%s_notif` | inside `#ifndef _WIN32` | Unchanged |
| `test_eventloop.c:610` | `/tmp/cobalt_unix_test.sock` | inside `#ifndef _WIN32` guards (11 instances) | Unchanged |
| `tests/benchmark/*` | `/tmp/%s_notif` | benchmark targets run only on Linux runners | Unchanged |

Only `test_logger.c` required repair. The remaining sites are already POSIX-guarded or
benchmark-only, so no further code change — but the convention must be written down so
that the next unguarded path is caught by review:

**Convention (to be recorded in the spec repo and reflected in `AGENTS.md`'s
cross-platform checklist):** tests MUST NOT hardcode absolute POSIX paths. Use
`cobalt_test_tmpfile()` when a scratch file is needed. Where an absolute path is
genuinely unavoidable (UNIX-domain sockets, `/dev/shm`), the code MUST sit inside
`#ifndef _WIN32` with a documented counterpart branch.

## 6. Part 3 — Coverage Backfill

### 6.1 Triage of the 31 candidate symbols

**Class A — partially covered; add boundary assertions only.** Most of these are reached
by every existing test of its module via the `cobalt_X_create()` →
`cobalt_X_create_with_allocator(system)` delegation, so the happy path is not dark. What
is dark is the **non-default path**: the NULL-allocator branch, and whether allocations
actually flow through the injected allocator rather than a mixture of it and an internal
default.

| Symbol | Module | Verified NULL-allocator behavior | Assertion to add |
|---|---|---|---|
| `cobalt_arena_create_with_allocator` | memory/arena | returns NULL | NULL → NULL; system allocator round-trip |
| `cobalt_lfqueue_create_with_allocator` | container | returns NULL | NULL → NULL; system allocator round-trip |
| `cobalt_treemap_create_with_allocator` | container | returns NULL | NULL → NULL; system allocator round-trip |
| `cobalt_set_create_ext_with_allocator` | container | returns NULL | NULL → NULL with custom comparator; system allocator round-trip |
| `cobalt_eventloop_create_with_allocator` | module | returns NULL | NULL → NULL; system allocator round-trip paired with destroy |
| `cobalt_taskgraph_create_with_alloc` | module | **falls back to `malloc`** | NULL → non-NULL (documented `malloc` fallback); custom allocator used when given |
| `cobalt_waitgroup_create_with_alloc` | module | **falls back to system allocator** | NULL → non-NULL (system fallback); custom allocator round-trip |
| `cobalt_tsdeque_create_with_allocator` (+ 6 siblings: tshashmap, tslist, tsqueue, tsset, tsstack, tstreemap) | container | returns NULL | NULL → NULL; system allocator round-trip. `cobalt_tsvector_create_with_allocator` is excluded — already covered by `test_threadsafewrapper_vector_allocator` |
| `cobalt_tstreemap_create_ext` | container | delegates to `cobalt_tstreemap_create()` | custom comparator accepted |
| `cobalt_tstreemap_create_with_allocator` | container | returns NULL | NULL → NULL; system allocator round-trip |

**Two distinct NULL-allocator contracts exist, and the tests must encode both
explicitly rather than assuming one convention:**

1. **Strict — NULL rejected.** arena, lfqueue, treemap, set (`_ext_with_allocator`),
   eventloop, every thread-safe wrapper family. Returns NULL.
2. **Permissive — NULL tolerated.** `cobalt_taskgraph_create_with_alloc` falls back to
   plain `malloc`; `cobalt_waitgroup_create_with_alloc` falls back to
   `cobalt_allocator_get_system()`. Returns non-NULL.

This asymmetry is easy to get wrong and is exactly why these paths need assertions:
a contributor "fixing" the strict functions to fall back (or vice versa) would otherwise
change behavior with no test noticing. Each function's observed behavior in this table
was read from its implementation, not inferred from naming.

**Class B — no delegation chain; real dark spots.** Behavior verified by reading each
implementation before writing the test.

| Symbol | Verified behavior to assert |
|---|---|
| `cobalt_taskgraph_reset` | Restores each node's `remaining_in_degree` to `static_in_degree` (atomics). Assert NULL no-op; after reset, a consumed graph can be walked again |
| `cobalt_eventloop_accept` | Non-blocking accept; NULL guard; returns 0 on success, -1 on EAGAIN and when NULL |
| `cobalt_set_contains_ext` / `cobalt_set_insert_ext` / `cobalt_set_remove_ext` | Binary-key insert/contains/remove round-trip on an `_ext`-created set |
| `cobalt_set_get` | Returns the map-interface sentinel for an existing member, NULL for an absent one; NULL guards |
| `cobalt_set_iterator_create` | Yields all members; NULL guard; destroyed via `cobalt_map_iterator_destroy` |
| `cobalt_logger_log` | Direct call (no macro) reaches the same formatting path as the macros |
| `cobalt_vformat_with_alloc` | Custom allocator allocated the output string; both format paths |

**Class B, reduced by verification:**

| Symbol | Finding | Disposition |
|---|---|---|
| `cobalt_list_merge_sort` | Body is arg validation + `(void)count;` and `return;` — a **no-op stub** in `src/container/list.c` | Assert only the NULL guards (`head_ref == NULL`, `compar == NULL`). Do not assert sorting; the function does not sort. Flagged as a product gap, out of scope here |

**Class C — excluded, with reason recorded so the decision is not re-litigated:**

| Symbol | Exclusion reason |
|---|---|
| `cobalt_bswap16_generic`, `cobalt_bswap32_generic`, `cobalt_bswap64_generic` | Internal implementation detail in `platform/utils.h`, not a stable API surface; the byte-swap behavior is covered through the wrapped non-generic functions |
| `cobalt_ebr_get_id` | Trivial getter over an internal slot id; meaningful testing requires multi-thread construction, which is YAGNI here |
| `cobalt_fatal` | Emits and calls `exit(1)`. Testing it requires `fork`/`waitpid` isolation, explicitly deferred. Assert only that the macro/function is reachable at compile time; never execute it |

### 6.2 Placement

No new test files. Each case goes into the existing `tests/unit/test_<module>.c`
matching the module, wired into that file's existing entry function. This preserves the
project's one-file-per-module convention and keeps `ctest -R test_<name>` working as a
targeted verification command.

### 6.3 Assertion discipline

- One `TEST_ASSERT` or `TEST_EQUAL` per observable contract, not per line executed.
- No test of incidental behavior, wording, or internal field layout.
- Every new allocation path gets a matching free/destroy so ASan/LSan stays clean.

## 7. Rejected Alternative: Compile-Time Coverage Enforcement

Parsing public headers into a symbol table in CMake and failing the build when a symbol
has no test reference was considered and rejected **for this round**. It would
immediately fail on the 31 existing gaps, so it can only ship after the backfill lands;
adding it now means either a red build or a suppression list that negates the value. It
is the natural follow-up once §6 is merged, and the backfill produces exactly the
baseline it would need.

## 8. Verification

Per commit:

1. `cmake --build . -j4` — no new warnings (`COBALT_STRICT_WARNINGS=ON` is default).
2. `ctest -R test_<name> --output-on-failure` — targeted module green.
3. `clang-format --dry-run --Werror --style=file` with **both** clang-format 18 (CI parity)
   and 22 (local): violation count must equal the file's baseline count, not zero.
   Known baselines: `test_threadsafewrapper.c` 42, `test_iterator.c` 1, `test_class.c` 1.
   Do not run `clang-format -i` on these files — it would reformat the pre-existing
   violations and pollute the diff.

End of round:

4. Full `ctest --timeout 120` → 63/63 in Debug.
5. ASan/LSan build (`-DCOBALT_RUN_ASAN=ON`), each changed filter → exit 0, 0 leaks.
   Detection must be proven with a positive control (inject a leak, confirm LSan reports
   it, revert), not assumed from a clean run.
6. CI: the `windows` jobs must go green. This is the acceptance criterion for §4 and
   cannot be verified locally (no MinGW toolchain on the workstation).

Baseline for this design: 63/63 locally, all CI jobs green except `windows`.

## 9. Commit Plan

| # | Message | Contents |
|---|---|---|
| 1 | `test(core): ✅ use portable tmpfile helper in logger tests` | `test_framework.h` helper + 6 `test_logger.c` sites |
| 2 | `docs(tests): 📝 record no-hardcoded-posix-path convention` | Convention in this spec's §5 mirrored into the test docs |
| 3 | `test(memory): ✅ assert allocator-injection boundary branches` | Class A: arena, lfqueue, treemap, eventloop, taskgraph, waitgroup |
| 4 | `test(module): ✅ cover taskgraph_reset and eventloop_accept` | Class B module symbols |
| 5 | `test(container): ✅ cover set ext API and ts allocator variants` | Class B container symbols + thread-safe wrapper family |

Ordering rationale: the Windows fix lands first to restore `master`; each subsequent
commit is independently revertible and independently verifiable.

## 10. Risks

| Risk | Mitigation |
|---|---|
| `tmpfile()` behaves differently on the MinGW runner | It is C89; if it still fails, the assertion names the failing call and the next hypothesis (runner temp dir permissions) can be tested directly. CI feedback is the gate |
| Backfill exposes a genuine product bug | That is the point of the work; the bug is recorded and reported, not papered over. The test asserting observed behavior is kept and the product bug is filed |
| `test_logger` global state leaks between the three test functions | Each function ends by restoring `cobalt_logger_init(stdout, LOG_LEVEL_INFO)`, already established by the P0 hardening; preserve that |
| clang-format 18/22 disagreement on new declaration groups | Keep new declaration blocks to single-line initializers; verify against both versions (§8.3) |
