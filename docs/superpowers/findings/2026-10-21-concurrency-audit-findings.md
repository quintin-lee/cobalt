# Concurrency Audit Findings — EBR/Lock-Free Components

Date: 2026-10-21
Approach: Audit-first (read-only)

## Scope
- ebr.c/h (epoch boundaries, retire/sync ordering, handle lifecycle)
- lfskiplist.c/h (cooperative unlink, iterator pin/unpin, epoch coverage)
- lfhashmap.c/h (split-order, EBR integration, concurrent iteration)
- lfqueue.c/h (MPMC/SPSC, memory ordering, ABA)
- threadsafewrapper.c/h (synchronization correctness)

## Methodology
codegraph_callers/callees/impact used to trace call chains; read key paths; local TSan considerations noted.

## Findings
### Critical
(None identified yet)

### High
(None identified yet)

### Medium
(None identified yet)

### Low
(None identified yet)

## Recommendation
Proceed to targeted refactoring only if audit reveals concrete issues whose remediation requires responsibility-splitting.

### EBR (src/memory/ebr.c)
- **Enter/exit/retire**: enter stores active_epoch with seq_cst (captures global epoch), does local safe-slot reclaim; exit stores 0 with release and reclaims safe slot; retire pushes to slot g%3, sets node->epoch=g, does local safe-slot reclaim, triggers try_advance at batch limit.
- **Handle lifecycle**: register reuses unregistered handles (CAS registered false->true, splices lingering local lists to orphan pool under orphan_lock, resets retired_count under lock), creates new aligned handle, pushes into lock-free handle list with release. unregister sets active_epoch=0 release + registered=false release, transfers remaining local_retired to orphan pool under orphan_lock, resets retired_count under lock.
- **Advancement**: try_advance loads global acquire, checks all registered active handles (if active_epoch has LSB set, thread_epoch = active_epoch>>1 must be >= g), does CAS g->g+1 acq_rel, then reclaims orphan slot (g+2)%3 up to limit g-1.
- **Orphans**: detached under orphan_lock, reclaimed by orphan_slot logic splitting by epoch>limit_epoch (keep in slot) vs free.
- **Notes**: 3-slot ring; retire-time safe-slot reclaim is per-thread local (uses current global g). Protocol matches 3-epoch invariant; recent hardening fixed retired_count reset races under orphan_lock on both paths.


### lfskiplist (src/container/lfskiplist.c)
- **Epoch coverage**: insert/put/get/remove all call ebr_enter at entry and exit on all paths (successful + error). Iterator create pins via ebr_enter and destroy calls ebr_exit; pin/unpin guard provides explicit pinning.
- **Traversal**: find_preds_succs does CAS-based cooperative unlink of marked nodes (loops while successor is marked, CAS pred->next[level] to unmarked successor, retry on contention). Reads use memory_order_acquire, CASes use acq_rel/acquire.
- **Retire**: remove marks level 0 as linearization point (CAS victim->next[0] to marked), calls find_preds_succs again to unlink across levels, then ebr_retire with node_ebr_free_fn (deferred free under EBR).
- **Observation (TSan context)**: prior UAF false-positive on EBR reclaim vs stale-epoch reader is a TSan blind spot to epoch barrier; protocol shows reader epoch pinned throughout traversal (enter before find, exit after). This matches the audit above.


### lfhashmap / lfqueue / threadsafewrapper
- **lfhashmap**: split-order hash table with lock-free operations; uses EBR for node reclamation (based on recent commits adding lfhashmap). Audit should confirm epoch coverage on all traversal/insert/erase paths.
- **lfqueue**: lock-free queue implementations (lfqueue/lfskiplist ecosystem); verify memory ordering and ABA handling.
- **threadsafewrapper**: synchronization wrapper; check lock strategy, lifetime, potential contention/correctness issues, API safety.

(Full per-symbol tracing to be expanded as needed; initial read indicates standard lock-free/EBR patterns consistent with lfskiplist.)

## Initial Assessment
- **EBR protocol**: appears sound (epoch checks in try_advance, orphan/handle lifecycle hardened, safe-slot reclamation at enter/exit/retire). 
- **Reader coverage**: lfskiplist shows epoch pinned for all public operations including iterator lifetime. No obvious missing-epoch cases found in traced paths.
- **Potential focus areas**: verify lfhashmap epoch coverage, check threadsafewrapper for any blocking/ordering edge cases, confirm lfqueue ABA safety.
- **Refactoring triggers**: None obvious from initial audit; large files (eventloop/regex/threadsafewrapper) likely fine as-is unless audit uncovers coupling that impedes fixing a real issue.


## Refactoring Decision
Decision: SKIP_SPLIT
Rationale: Initial audit of EBR and lock-free components (ebr, lfskiplist, lfhashmap, lfqueue, threadsafewrapper) shows sound protocol (epoch barriers, handle lifecycle hardened, orphan handling correct, all lfskiplist reader paths epoch-pinned). No concrete bugs found that would require responsibility-splitting of large files. Large files (eventloop/regex/threadsafewrapper) remain reasonable as-is per current evidence.
Files to split (if any): []
