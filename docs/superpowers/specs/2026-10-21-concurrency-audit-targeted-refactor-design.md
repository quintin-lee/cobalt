# Cobalt — Concurrency Robustness Audit + Targeted Maintainability Refactoring (Design)

**Date:** 2026-10-21  
**Status:** Draft — awaiting user review  
**Approach:** Audit-first (Scheme 1), C+D combined  
**Follows:** [2026-10-20 lock-free hashmap design](./2026-10-20-lockfree-hashmap-design.md), [2026-10-19 tsan-flake CI reliability design](./2026-10-19-ci-reliability-tsan-flake-design.md)

## 1. Goals and Non-Goals

### Goals
- **(D) Concurrency robustness**: Audit EBR protocol and lock-free components (`ebr.c`, `lfskiplist.c`, `lfhashmap.c`, `lfqueue.c`, `threadsafewrapper.c`). Focus on epoch boundaries, retire/sync ordering, handle registration/unregistration/reuse, iterator lifetime, and cooperative unlink.
- **(C) Maintainability**: Only split large files where the audit reveals responsibility mixing or where splitting meaningfully reduces complexity without changing public APIs. Prioritize `threadsafewrapper.c`, `eventloop.c`, `regex.c` if warranted.
- **Conservative baseline**: Keep all public APIs stable, behavior unchanged, all tests passing (61/61).

### Non-Goals
- Full-scale splitting of all three large files ("split for the sake of splitting")
- Performance micro-optimizations (unless directly tied to a correctness issue found during audit)
- New features
- Large API changes/renames

## 2. Audit Plan (D) — Read-Only First

The audit is **read-only by default**. Code changes only occur if concrete bugs are found and require fixes.

### Methodology
1. **Call graph tracing**: Use `codegraph_callers`, `codegraph_callees`, `codegraph_impact` to map EBR-related symbols and their dependencies.
2. **Protocol replay**: Trace `cobalt_ebr_enter`/`exit`/`retire`/`try_advance`/`sync` paths. Verify the 3-epoch invariant and handle lifecycle (`register`/`unregister`/reuse).
3. **Iterator & cooperative paths**: Audit `lfskiplist`/`lfhashmap` iterators for pin/unpin coverage, epoch lifetime, and cooperative unlink ordering.
4. **Memory ordering & races**: Check `memory_order` usage, cross-thread pointer lifetimes, orphan lists, batch counters, and potential TOCTOU windows.
5. **Empirical validation**: When plausible, run local TSan builds against relevant stress tests to distinguish real races from TSan's known blind spots (e.g., EBR epoch barriers).

### Audit Scope
| Component | Focus Areas |
|-----------|-------------|
| `src/memory/ebr.c` | Global epoch advancement, safe-slot reclamation, orphan handling, handle reuse, sync semantics |
| `src/container/lfskiplist.c` | Mark-and-sweep unlink, cooperative unlink, iterator pinning, retire timing |
| `src/container/lfhashmap.c` | Split-order table, EBR integration, bucket/chain lifetime, concurrent iteration |
| `src/container/lfqueue.c` | MPMC/SPSC semantics, memory ordering, ABA hazards |
| `src/container/threadsafewrapper.c` | Synchronization primitives, locking strategy, potential contention/correctness issues |

### Deliverable
Audit findings document (embedded in design or separate report) containing: file:line, call chain, severity (Critical/High/Medium/Low), evidence, classification (bug vs false-positive), and concrete fix suggestions.

## 3. Targeted Refactoring Plan (C) — Only If Warranted

**Rule:** Refactoring is **conditional** on audit findings. If the audit finds no strong justification for splitting, **skip this entirely**.

### Trigger Conditions
- Responsibility mixing that directly impedes fixing a concurrency bug found in the audit
- Excessive coupling that makes the audited component untestable or unreasonably hard to reason about
- Clear single-responsibility boundaries become obvious from the audit

### Candidate Files (in priority order)
| File | LOC | Rationale |
|------|-----|-----------|
| `src/container/threadsafewrapper.c` | ~1214 | Often contains multiple synchronization strategies; strong candidate if audit shows coupling issues |
| `src/runtime/eventloop.c` | ~1865 | Timers, I/O, signals, callbacks often interleaved; split only if audit touches this area |
| `src/algorithm/regex.c` | ~1635 | Parser/codegen/Pike VM boundaries could be cleaner, but less directly tied to concurrency audit |

### Constraints
- **API stability**: All public headers (`include/cobalt/**/*.h`) must remain unchanged. No exported symbol renames/removals.
- **Behavior preservation**: Zero functional changes. Pure structural moves.
- **Incremental validation**: Build and test after each split to catch issues early.
- **Follow existing patterns**: Match current style, include guards, layer dependencies.
- **Minimal diffs**: Only touch what's necessary.

## 4. Verification & Acceptance Criteria

| Check | Criteria |
|-------|----------|
| **Audit completeness** | Every component in scope reviewed with concrete evidence; findings classified and actionable |
| **Test suite** | All existing tests pass (61/61) at every verification checkpoint |
| **Sanitizers** | Affected modules clean under ASan; no new TSan reports beyond known false-positive classes |
| **Warnings** | Zero new compiler warnings |
| **API compatibility** | Public APIs unchanged, no ABI breaks |
| **Scope discipline** | If no justification exists for splitting, C phase skipped entirely |
| **Traceability** | All refactoring driven by audit findings or clearly justified |

## 5. Implementation Strategy

1. **Phase 1 (Audit)**: Read-only exploration using `codegraph_*` tools, trace key paths, document findings. Run targeted TSan if needed.
2. **Phase 2 (Decide)**: Review audit findings. If splitting is warranted, proceed to Phase 3 for only those specific files/modules. Otherwise stop at audit conclusions.
3. **Phase 3 (Targeted Refactor)**: For approved splits only, extract by responsibility while preserving includes/dependencies and public interfaces. Test after each change.
4. **Phase 4 (Verify)**: Full test suite + ASan on affected areas. Document any fixes made.

## 6. Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Audit finds no refactoring needed (most likely) | Low | Acceptable—primary goal (concurrency robustness) still met; produce actionable audit report |
| Splitting introduces subtle bugs | High | Interface stability enforced, test after every change, ASan verification, minimal scope |
| Circular dependencies during split | High | Design interfaces first, follow existing layer rules (upward-only dependencies) |
| Touching hot concurrency paths | High | Audit-first means we understand them fully before moving code; prefer read-only changes where possible |

## Appendix: Key References
- EBR implementation: `src/memory/ebr.c`, `include/cobalt/memory/ebr.h`
- Lock-free components: `src/container/lfskiplist.c`, `src/container/lfhashmap.c`, `src/container/lfqueue.c`
- Large files under review: `src/container/threadsafewrapper.c`, `src/runtime/eventloop.c`, `src/algorithm/regex.c`
- Existing design specs in `docs/superpowers/specs/`
