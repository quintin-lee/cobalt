# Cobalt Benchmark Baseline & Hot-Path Analysis — Specification

**Date:** 2026-10-31  
**Status:** Draft  
**Scope:** Benchmark regression gate + hot-path analysis infrastructure  
**Authors:** Antigravity & Cobalt Maintainer

---

## 1. Overview & Goals

Cobalt already ships a unified benchmark runner (`tests/benchmark/bench_runner.c`) with 31 benchmarks and a `--json` mode emitting `{name, mean_ms}` per point. The CI `bench` job compares the latest `result.json` against a sliding artifact baseline (`benchmark-baseline`) and fails on any benchmark regressing >5%.

**Problems this spec addresses:**

1. **Baseline is not reproducible.** The baseline lives only in CI artifacts; a developer cannot run the regression check locally against a known-good snapshot.
2. **No improvement detection.** Only regressions (>+5%) are flagged; genuine improvements (>-5%) are silently overwritten by the next upload.
3. **No hot-path profiling hook.** CI reports *that* a benchmark regressed but not *which function* inside it is responsible. Optimization is guesswork.

**Goals:**

1. Commit a `tests/benchmark/baseline.json` snapshot checked into the repo as the reproducible baseline.
2. Add `compare_bench.py` to compare `result.json` against `baseline.json`: flag regressions (>+5%) as failures, record improvements (>-5%) in CI summary.
3. Harden the CI `bench` job so both the artifact comparison (existing behavior) and the committed-baseline comparison run; either regression fails the job.
4. Document a profile-driven hot-path analysis workflow (`perf` / `callgrind` per bench point) to locate the responsible function before optimizing.

**Non-goals (out of scope for this spec):** product-code optimizations of sort/hashmap/lfqueue themselves; those are per-point decisions made after profiling. Container/memory/concurrency implementation designs already live in `2026-10-15-core-containers-and-memory-optimization-design.md`.

---

## 2. Architecture

All components live under `tests/benchmark/` except the CI job in `.github/workflows/ci.yml`.

### Component A — `baseline.json` (new, committed)

Format (produced by `bench_runner --json`):

```json
{
  "benchmarks": [
    {"name": "vector_push", "mean_ms": 0.842, ...},
    {"name": "hashmap_put", "mean_ms": 1.210, ...},
    ...
  ]
}
```

Generated once from a clean Release build and committed. Replaced only after a reviewed performance change that intentionally shifts numbers.

### Component B — `compare_bench.py` (new)

Inputs: `baseline.json` (committed) and `result.json` (fresh `bench_runner --json` output).

Logic per shared benchmark name:

- `pct = (cur - base) / base * 100`
- `pct > +5` → **regression**: fail (`sys.exit(1)`), print name + old → new + pct.
- `pct < -5` → **improvement**: record to CI summary, do not fail.
- `base == 0` or missing base → skip, summary-recorded.
- Name in result but not baseline → new bench, summary-recorded, no fail.
- Name in baseline but not result → removed bench, summary-recorded, no fail.

Exit code: 0 if no regressions; 1 if any regression; 2 on parse/IO error.

### Component C — CI `bench` job (hardened)

Existing steps retained: Release build with clang, `bench_runner --json > .benchmarks/result.json`, download previous artifact baseline (`continue-on-error`), compare against it, upload result as new artifact.

**Added:** compare committed `tests/benchmark/baseline.json` against `.benchmarks/result.json` via `compare_bench.py`. If either comparison finds a regression, the job fails.

### Data flow

```
bench_runner --json  →  .benchmarks/result.json
                                                →  compare_bench.py
baseline.json  ────────────────────────────────→  (regression / improvement report)
                                                →  CI bench job pass/fail
```

---

## 3. Error Handling

| Scenario | Behavior |
|---|---|
| `baseline.json` missing (first commit) | `compare_bench.py` prints `No baseline found, recording…`, exit 0; CI passes |
| New bench name in result (not in baseline) | Summary-recorded, not a regression |
| Baseline name missing from result | Summary-recorded as removed, not a regression |
| `mean_ms == 0` | Skipped (avoid divide-by-zero), summary-recorded as `skipped` |
| JSON parse error in either file | Exit code 2, CI fails with hint to check `bench_runner --json` output |
| CI artifact download fails | Allowed (`continue-on-error`); falls back to committed `baseline.json` |

---

## 4. Testing Strategy

1. **Script-level verification (local, no CI):** Generate a `result.json` from the local Release build. Hand-craft two `baseline.json` variants: one with a `+6%` entry (expect `compare_bench.py` exit 1 and the name listed), one with a `-6%` entry (expect exit 0 and an improvement line).
2. **CI gate verification (local simulation):** Run `compare_bench.py` against a committed `baseline.json` and a fresh `result.json`; confirm exit code matches expectation.
3. **Performance-change verification:** After any intentional product-code optimization tied to a bench point, re-run that point, confirm `mean_ms` does not regress >5% and record improvement if applicable, then update both `baseline.json` and the CI artifact baseline.

---

## 5. Hot-Path Analysis Workflow

When `compare_bench.py` flags a regression on point X:

1. Build the same bench binary with Debug symbols (`-g -O0` or `-g -Og`) to get usable stack symbols.
2. Run `perf record -e cpu-clock ./bench_runner --filter X` (or isolate the bench function), then `perf report` to see hot functions/lines.
3. Alternatively, `valgrind --tool=callgrind ./bench_runner --filter X` → `kcachegrind` view for call-graph attribution.
4. Decide the optimization target (e.g. sort comparison, hashmap hash, lfqueue CAS retry), implement, re-run the bench, and update `baseline.json` only on reviewed improvement.

---

## 6. Scope Check

This spec covers the **measurement gate and analysis workflow only**. Any actual product-code change triggered by a hot-path finding (e.g. rewriting a sort comparison) is a separate implementation cycle with its own tests and benchmarks, and must not regress any other bench point by >5%.
