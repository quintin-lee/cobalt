# Cobalt CI Bench Gate Strengthening — Specification

**Date:** 2026-11-02  
**Status:** Draft  
**Scope:** CI `bench` job regression gate  
**Authors:** Antigravity & Cobalt Maintainer

---

## 1. Overview & Goals

The CI `bench` job compares `bench_runner --json` output against a baseline using `compare_bench.py`. The current regression threshold is >+5% on `mean_ms`. Benchmarks are inherently noisy (CPU scheduling, cache effects), so a 5% threshold can both miss subtle regressions and flake on benign noise.

**Goals:**

1. **Lower the regression threshold** from +5% to +3% to catch subtle performance regressions earlier.
2. **Add an absolute-time hard cap** (e.g., any benchmark exceeding 500ms fails regardless of %) to prevent pathological slowdowns that percentage thresholds might miss on already-fast benchmarks.
3. **Keep improvement recording** (>-5%) unchanged so genuine improvements are still visible in CI summary.
4. **Keep the committed-baseline comparison** (artifact baseline as fallback) unchanged.

**Non-goals:** Changing `compare_bench.py` output format; adding new benchmarks; changing ASan/UBSan/TSan/valgrind/coverage gates.

---

## 2. Architecture

### Component A — `compare_bench.py` threshold change

Change the regression condition from `pct > 5.0` to `pct > 3.0`. Add an absolute cap: if `cur_ms > 500.0`, fail regardless of percentage.

```python
if pct > 3.0:
    regressed.append(...)
if cur_ms > 500.0:
    regressed.append((name, base_ms, cur_ms, pct, "absolute_cap"))
```

Both conditions are independent; either triggers a regression.

### Component B — CI `bench` job (no structural change)

The existing `Compare against committed baseline` step already calls `compare_bench.py`. No CI YAML change is needed unless `compare_bench.py` CLI changes (it doesn't). The artifact-baseline comparison step remains as the fallback.

### Data flow

```
bench_runner --json  →  result.json
result.json  →  compare_bench.py (baseline.json)  →  regression (>3% or >500ms) / improvement (>-3%)
compare_bench.py exit code  →  CI bench job pass/fail
```

---

## 3. Error Handling

| Scenario | Behavior |
|---|---|
| `cur_ms > 500.0` (any benchmark) | Regression, exit 1, regardless of % |
| `pct > 3.0` (shared benchmark) | Regression, exit 1 |
| `pct < -3.0` (shared benchmark) | Improvement, recorded, no fail |
| `base == 0` or missing base | Skip, summary-recorded |
| Parse/IO error | Exit code 2, CI fails |

---

## 4. Testing Strategy

1. **Script-level verification (local):** Generate a `result.json` from the local Release build. Hand-craft `baseline.json` variants:
   - One with a shared benchmark at +4% (expect exit 1, regression listed).
   - One with a shared benchmark at +2% (expect exit 0, no regression).
   - One with a benchmark at 501ms (expect exit 1, absolute cap triggered).
2. **CI gate verification:** Run `compare_bench.py` against committed `baseline.json` and fresh `result.json`; confirm exit code matches expectation.
3. **Full suite:** `ctest --output-on-failure --timeout 120` must remain 63/63.

---

## 5. Scope Check

- **In scope:** `compare_bench.py` threshold + absolute cap; CI bench job remains calling the same script.
- **Out of scope:** ASan/UBSan/TSan/valgrind/coverage gates (separate spec if needed); changing `compare_bench.py` CLI; adding benchmarks.

This is focused enough for one implementation plan.

---

## 6. Ambiguity Check

- **"500ms absolute cap":** applies to any benchmark's `mean_ms`, regardless of baseline. A fast benchmark that suddenly jumps to 600ms fails even if the % increase is small (e.g., from 100ms to 600ms is +500%, already caught by % rule; but 400ms to 501ms is +25%, also caught; the absolute cap mainly guards against a benchmark that was already slow getting slower by a small % but crossing a user-facing threshold).
- **Improvement threshold stays at -3.0%:** only the regression threshold changes to 3.0%; improvement recording remains as-is to avoid noise.
