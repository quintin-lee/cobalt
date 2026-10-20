# Cobalt — Targeted Maintainability Refactoring (Design)

**Date:** 2026-10-21  
**Status:** Draft — awaiting user review  
**Scope:** Code quality/maintainability  
**Approach:** Conservative, behavior-preserving, API-stable

## 1. Goals

Reduce file size and complexity via **minimal, targeted splits**:
- `src/module/eventloop.c` (1865 LOC) — split along natural responsibility boundaries if clear; otherwise defer
- `src/algorithm/regex.c` (1635 LOC) — split by compilation/execution phases if natural (lexer/parser/AST/codegen/execution)
- `src/container/threadsafewrapper.c` (1214 LOC) — split by primitive vs wrapper concerns if mixed

**Hard constraints:**
- Behavior unchanged (no semantic changes)
- All public APIs, headers, and exported symbols remain stable (no API/ABI breaks)
- All existing tests pass (63/63)
- Zero new compiler warnings; clang-format/tidy clean
- ASan clean on affected modules

## 2. Principles

- **Split only when boundaries are clear.** If no clean boundary exists, do not split (YAGNI).
- **Internal refactor only.** No public API surface changes; include paths preserved where possible.
- **Minimal diffs.** Move code, don’t rewrite logic.
- **Incremental verification.** Build and test after each split.
- **Conservative scope.** Prefer doing nothing if the gain is marginal.

## 3. Candidate Boundaries

| File | Candidate Split Points | Rationale |
|---|---|---|
| `threadsafewrapper.c` | Low-level primitives vs higher-level synchronization wrappers | Separates concerns; may improve testability without changing API |
| `eventloop.c` | Timer management (heap/timer wheel), event loop core (poll/epoll/kqueue), signal/IO handling | Natural subsystems; split only if clearly separable with minimal coupling |
| `regex.c` | Tokenization/lexer, parsing/AST construction, codegen, Pike VM execution | Classic compiler phases; internal helpers can move without affecting public API |

Splits will **only** be performed where the boundary is clean and does not introduce circular dependencies or require header churn.

## 4. Validation & Acceptance Criteria

- Full test suite: `ctest` reports **63/63 passing**
- Static analysis: `clang-format` and `clang-tidy` clean (no new warnings)
- Sanitizers: ASan clean on affected modules
- Build: clean Debug build, no new warnings
- Behavior: no functional changes; public API unchanged

## 5. Out of Scope

- Broad, cross-cutting refactors unrelated to reducing these three files
- API renames, header reorganization beyond what’s strictly necessary for internal splits
- Performance changes
