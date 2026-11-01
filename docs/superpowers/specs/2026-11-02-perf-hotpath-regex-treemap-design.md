# Cobalt Performance Hot-Path Optimization (regex_ipv4 / treemap) — Specification

**Date:** 2026-11-02  
**Status:** Draft  
**Scope:** Performance optimization of hot benchmark paths  
**Authors:** Antigravity & Cobalt Maintainer

---

## 1. Overview & Goals

Cobalt already has a benchmark gate: `bench_runner --json` produces per-benchmark `mean_ms`, compared against a committed `tests/benchmark/baseline.json` via `compare_bench.py`. Regressions (>+5%) fail CI; improvements (>-5%) are recorded.

**Current baseline (top hot paths):**

| Benchmark | mean_ms |
|---|---|
| taskgraph_dag | 223.9 |
| treemap_get | 173.8 |
| regex_ipv4 | 145.0 |
| hashmap_get | 121.9 |
| treemap_put | 106.7 |

**Goals:**

1. **Profile** the top remaining hot benchmarks (`regex_ipv4`, `treemap_get/put`, `hashmap_get/put`) with `perf` / `callgrind` to identify the responsible functions.
2. **Optimize** the top 1–3 hot functions (candidate targets: `rb_find`/`rb_compare` in treemap, regex IPv4 matching loop, hashmap hash/compare).
3. **Verify** with `compare_bench.py`: no regression (>+5% fails CI); record improvements (>-5%) and update `baseline.json` only for reviewed improvements.

**Non-goals:** Adding new benchmarks; changing public API; optimizing benchmarks already fast (<10ms); redesigning treemap/regex algorithms (only hot-function micro-optimizations).

---

## 2. Architecture

All components under `tests/benchmark/` and `src/` product code:

### Component A — Profiling workflow

For each hot benchmark point X:

1. Build Debug-with-symbols binary (`-g -Og` or `-g -O0` for usable stack symbols).
2. `perf record -e cpu-clock ./bench_runner --filter X`
3. `perf report` to see hot functions/lines.
4. Alternative: `valgrind --tool=callgrind ./bench_runner --filter X` → `kcachegrind` view for call-graph attribution.

### Component B — Optimization target

Based on profiling, pick 1–3 hot functions in `src/`:
- `src/container/treemap.c` — `rb_find`/`rb_compare` (treemap_get/put: 173.8ms/106.7ms)
- `src/algorithm/regex.c` — regex IPv4 matching loop (regex_ipv4: 145.0ms)
- `src/container/hashmap.c` — hashmap hash/compare (hashmap_get/put: 121.9ms/63.1ms)

### Component C — Verification gate

After each optimization:

1. Rebuild Release.
2. Run the affected benchmark point.
3. `compare_bench.py tests/benchmark/baseline.json .benchmarks/result.json` — expect no regression; if improvement >5%, update `baseline.json` and commit.
4. Run full ctest suite to ensure no functional regression.

### Data flow

```
perf/callgrind profile  →  hot function identified
hot function optimized  →  bench_runner --json → result.json
result.json  →  compare_bench.py vs baseline.json  →  no regression / improvement recorded
improvement  →  baseline.json updated + committed
```

---

## 3. Error Handling

- If profiling shows no clear hot function (flat profile), abort optimization for that point; don't guess.
- If an optimization causes >5% regression on any benchmark, revert that change before committing.
- If `compare_bench.py` fails, the commit is not made until fixed.
- Product code changes must keep `ctest --output-on-failure --timeout 120` green (63/63).

---

## 4. Testing Strategy

Per optimization target:

1. Profile → identify hot function.
2. Implement optimization in product code.
3. Build Release, run affected benchmark point(s).
4. `compare_bench.py` — confirm no regression.
5. Full ctest suite — confirm 63/63 green.
6. If improvement >5%, update `baseline.json` and commit both product code + baseline.

Full suite verification after all optimizations: `ctest --output-on-failure --timeout 120`.

---

## 5. Scope Check

- **In scope:** profiling top benchmarks, optimizing hot product-code functions, bench regression gate verification, baseline.json updates.
- **Out of scope:** new benchmarks, API changes, container algorithm redesigns (those are separate specs).

This is focused enough for one implementation plan; the profiling targets are independent of each other.

---

## 6. Ambiguity Check

- **"Top hot benchmarks":** defined as the remaining top by mean_ms (regex_ipv4, treemap_get/put, hashmap_get/put) after the previous taskgraph_dag optimization. If profiling reveals a different hotspot, that one takes precedence.
- **"Top 1–3 hot functions":** profiling decides which functions; if fewer than 3 show clear hot spots, optimize fewer.
- **"Improvement":** >-5% on the affected benchmark point with no regression on any other point. baseline.json only updated for reviewed improvements.
