#!/usr/bin/env python3
"""Compare bench_runner --json output against a committed baseline.

Exit 1 if any shared benchmark regresses by more than 5%.
Record improvements (>-5%) to stdout; do not fail on them.
Exit 2 on parse/IO error.
"""
import json
import sys


def load(path):
    try:
        with open(path) as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError) as exc:
        print(f"compare_bench: cannot read {path}: {exc}", file=sys.stderr)
        sys.exit(2)
    return {b["name"]: b["mean_ms"] for b in data.get("benchmarks", [])}


def main():
    if len(sys.argv) != 3:
        print("usage: compare_bench.py <baseline.json> <result.json>", file=sys.stderr)
        sys.exit(2)
    base = load(sys.argv[1])
    curr = load(sys.argv[2])
    regressed = []
    improved = []
    skipped = []
    for name, cur_ms in curr.items():
        if name not in base:
            print(f"compare_bench: new bench (no baseline): {name} = {cur_ms:.3f}ms")
            continue
        base_ms = base[name]
        if base_ms <= 0:
            skipped.append(name)
            continue
        pct = (cur_ms - base_ms) / base_ms * 100.0
        if pct > 5.0:
            regressed.append((name, base_ms, cur_ms, pct))
        elif pct < -5.0:
            improved.append((name, base_ms, cur_ms, pct))
    for name, old_ms, new_ms, pct in regressed:
        print(f"REGRESSION: {name}: {old_ms:.3f}ms -> {new_ms:.3f}ms (+{pct:.1f}%)")
    for name, old_ms, new_ms, pct in improved:
        print(f"IMPROVEMENT: {name}: {old_ms:.3f}ms -> {new_ms:.3f}ms ({pct:+.1f}%)")
    for name in skipped:
        print(f"SKIPPED (zero baseline): {name}")
    if regressed:
        print("compare_bench: performance regression detected", file=sys.stderr)
        sys.exit(1)
    print("compare_bench: no significant regression")
    sys.exit(0)


if __name__ == "__main__":
    main()
