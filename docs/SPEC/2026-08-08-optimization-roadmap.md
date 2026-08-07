# SPEC: Cobalt Framework Optimization Roadmap (2026-Q3)

**Date:** 2026-08-08  
**Scope:** P1–P5, ordered by priority and dependency  
**Status:** Draft — awaiting review

---

## 1. Background

Cobalt is a zero-dependency C11 framework (6,400 LOC, 32 source files, 30 test files, 37 ctest targets, 195 exported symbols). CI is green across 5 jobs (build/asan/ubsan/valgrind/coverage) with ccache. The framework is feature-complete at v2.2.0; this roadmap targets production-readiness improvements.

---

## 2. P1: Benchmark Framework

### 2.1 Current State
- 8 standalone benchmark executables in `tests/benchmark/` (vector, hashmap, treemap, list, deque, set, map, set_map)
- Each uses a hand-rolled `current_time_ms()` helper with `CLOCK_MONOTONIC`
- No statistical analysis (mean/stddev/percentiles)
- No baseline comparison or regression detection
- No CI integration for performance tracking

### 2.2 Design

**Option A: Inline lightweight framework (recommended)**
- Create `include/cobalt/utils/benchmark.h` + `src/utils/benchmark.c`
- Provide `BENCH_RUN(name, iterations, block)` macro that auto-runs warmup + N iterations
- Report: mean, stddev, min, max, ops/sec
- Add `bench_runner.c` test that aggregates all benchmarks with JSON output
- CI adds a `bench` job that runs benchmarks and compares against saved baseline in `.benchmarks/`

**Option B: External dependency (Google Benchmark)**
- Add as subdirectory or fetched via FetchContent
- More feature-rich but violates zero-dependency principle
- Requires C++ for full feature set

**Option C: Simple JSON CSV output only**
- Minimal change: add `--output=json` flag to existing benchmarks
- No new framework code
- Loses statistical rigor

### 2.3 Recommendation: Option A
- Preserves zero-dependency principle
- ~200 LOC new code
- Enables CI regression detection
- Extensible for future additions

### 2.4 Acceptance Criteria
- [ ] `bench_runner` executable runs all 8 benchmarks
- [ ] Output includes mean/stddev/min/max for each benchmark
- [ ] CI `bench` job runs and stores baseline as artifact
- [ ] PR with performance change > 5% triggers warning
- [ ] All benchmark code passes clang-tidy

---

## 3. P2: Event Loop Enhancement & Testing

### 3.1 Current State
- `eventloop.c` is 594 LOC, the 3rd largest source file
- Test coverage: 273 LOC test file, only 6 `TEST_ASSERT` calls
- Missing: timeout tests, signal handling tests, edge cases (FD reuse, timer cancel, nested run)
- No event loop stress test under high FD count

### 3.2 Design

**3.2.1 Timeout/Timer Tests**
- Test single-shot timer fires exactly once
- Test interval timer fires repeatedly
- Test timer cancellation mid-loop
- Test timer precision (±1ms tolerance)
- Test 1000 concurrent timers

**3.2.2 Signal Handling Tests**
- Test SIGTERM registration and handler invocation
- Test signal during active event loop

**3.2.3 Edge Cases**
- Test FD reuse after close/re-add
- Test `mod_fd` on non-existent FD
- Test `del_fd` on already-removed FD
- Test nested `eventloop_run` (should return error)
- Test empty run (no FDs, no timers, immediate return)

**3.2.4 Stress Test**
- 1024 FDs with periodic read/write
- 100 timers with varying intervals
- Mixed FD + timer workload for 10 seconds

### 3.3 Acceptance Criteria
- [ ] Event loop test file ≥ 15 `TEST_ASSERT` calls
- [ ] Timeout precision within ±2ms
- [ ] 1000-timer test passes within 5s
- [ ] All edge cases return documented error codes
- [ ] No memory leaks under Valgrind

---

## 4. P3: ABI Compatibility Testing

### 4.1 Current State
- 195 exported symbols, no version script
- No ABI stability guarantee mechanism
- Header-only API surface is stable, but symbol layout can change
- No cross-compilation testing

### 4.2 Design

**4.2.1 Symbol Export Script**
- Create `cobalt.map` linker version script:
  ```
  {
    global: cobalt_*;
    local: *;
  };
  ```
- Add to CMakeLists.txt: `-Wl,--version-script=${CMAKE_SOURCE_DIR}/cobalt.map`

**4.2.2 ABI Snapshot Test**
- CI job that dumps all exported symbols (`nm -D` or `objdump -T`)
- Stores snapshot in `.abi-snapshot/`
- On each PR, compares against baseline
- Flags: new symbols, removed symbols, signature changes

**4.2.3 Cross-Compilation Check**
- Add GitHub Actions job with `crossbuild` matrix:
  - x86_64-linux-gnu (gcc)
  - aarch64-linux-gnu (gcc)
  - arm-linux-gnueabihf (gcc)
  - x86_64-apple-darwin (clang via osxcross or native)
- Only build (no test) to verify compilation

### 4.3 Acceptance Criteria
- [ ] Linker version script present and applied
- [ ] CI `abi` job checks symbol stability
- [ ] Cross-compilation job passes for 3+ targets
- [ ] No new symbols without version bump in changelog

---

## 5. P4: Allocator Injection Test Coverage

### 5.1 Current State
- `pool.c` and `slab.c` support custom allocator injection
- `test_allocator.c` only tests system allocator (4 assertions)
- No tests for: custom allocator failure, pool with injected allocator, slab with injected allocator, container with injected allocator
- `cobalt_allocator_set_custom()` doesn't exist — injection is via constructor parameter only

### 5.2 Design

**5.2.1 Custom Allocator Stub**
```c
typedef struct {
    cobalt_allocator_t base;
    int alloc_count;
    int free_count;
    int fail_next;  // return NULL on next alloc if 1
} mock_allocator_t;

void *mock_alloc(cobalt_allocator_t *self, size_t size);
void  mock_free(cobalt_allocator_t *self, void *ptr);
```

**5.2.2 Test Cases**
| Test | Description |
|------|-------------|
| `test_allocator_inject_pool` | Pool created with mock allocator, verify alloc/free counts |
| `test_allocator_inject_slab` | Slab with mock allocator, verify size-class allocation |
| `test_allocator_inject_vector` | Vector with custom allocator, verify resizing uses it |
| `test_allocator_inject_hashmap` | HashMap with custom allocator, verify bucket alloc |
| `test_allocator_alloc_failure` | Mock returns NULL → container creation returns error |
| `test_allocator_realloc_shrink` | Mock realloc with smaller size |
| `test_allocator_realloc_expand` | Mock realloc with larger size, data preserved |
| `test_allocator_zero_size` | alloc(0) behavior with custom allocator |

**5.2.3 Vector/Hashmap Injection Verification**
- Create vector with `mock_allocator_t`
- Push 1000 elements → trigger multiple reallocations
- Verify mock `alloc_count` matches expected reallocation count
- Verify no system malloc was called

### 5.3 Acceptance Criteria
- [ ] `test_allocator.c` ≥ 12 `TEST_ASSERT` calls
- [ ] All containers pass with mock allocator
- [ ] Allocation failure propagates as `COBALT_ERROR_OUT_OF_MEMORY`
- [ ] Valgrind reports zero leaks with custom allocator

---

## 6. P5: Documentation & API Completeness

### 6.1 Current State
- `docs/API/` has per-module markdown files
- `docs/SPEC/` has architecture specs
- No Doxygen-generated HTML
- No performance benchmark results in docs
- No migration guide from v1→v2
- `README.md` lacks performance section

### 6.2 Design

**6.2.1 Doxygen Configuration**
- Add `Doxyfile` to project root
- Configure: `OUTPUT_DIRECTORY = docs/doxygen`
- Enable: `EXTRACT_ALL = YES`, `WARN_AS_ERROR = YES`
- Add to CMakeLists.txt as `docs` target
- CI adds `docs` job that builds and validates

**6.2.2 Performance Section in README**
- Add benchmark results table (from P1 framework)
- Compare: cobalt vs stdlib (vector push, hashmap get)
- Platform: GitHub Actions ubuntu-latest, GCC 13

**6.2.3 Migration Guide**
- `docs/TUTORIAL/migration_v1_to_v2.md`
- Document breaking changes:
  - Allocator injection pattern changed
  - Error handling API changes
  - Container constructor signatures

**6.2.4 API Stability Document Update**
- Update `docs/API_STABILITY.md` with v2.2.0 changes
- Document deprecation policy

### 6.3 Acceptance Criteria
- [ ] Doxygen builds without warnings
- [ ] README has performance comparison table
- [ ] Migration guide covers all v1→v2 breaking changes
- [ ] API stability doc updated for v2.2.0

---

## 7. Execution Order & Dependencies

```
P1 (Benchmark) ──────────────────────────┐
                                          ├──→ CI (no hard dependencies)
P2 (Eventloop) ──────────────────────────┤
                                          ├──→ Can run in parallel with P1
P3 (ABI) ────────────────────────────────┤
                                          ├──→ Depends on P1 (benchmark output format)
P4 (Allocator tests) ────────────────────┘
                                          ├──→ Can run in parallel
P5 (Documentation) ──────────────────────┘
                                          ├──→ Depends on P1-P4 results
```

**Recommended order:**
1. **P1** — benchmark framework (enables P5 performance section)
2. **P2** — eventloop tests (independent)
3. **P3** — ABI testing (independent, but benefits from P1 baseline)
4. **P4** — allocator tests (independent)
5. **P5** — documentation (depends on P1-P4 results)

---

## 8. Commit Strategy

Each P item → 1-3 commits:
- `feat(...)` or `test(...)` for new code
- `chore(ci)` for CI additions
- `docs(...)` for documentation
- All commits in English per project convention
- Phase validation: build + ctest + format + tidy after each P

---

## 9. Success Metrics

| Metric | Current | Target |
|--------|---------|--------|
| Test assertions | ~150 | ~250 |
| Benchmark coverage | 0% | 8/8 containers |
| ABI stability check | none | CI-gated |
| Allocator test coverage | 4 assertions | ≥ 30 assertions |
| Docs coverage | partial | Doxygen + migration |
| CI jobs | 5 | 7 (add bench + abi) |
