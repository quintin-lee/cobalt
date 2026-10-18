# Cobalt — Robustness Hardening Sweep (Design)

Date: 2026-10-18
Status: Approved (scope A + lone-surrogate semantics)
Follows: `2026-10-15-core-containers-and-memory-optimization-design.md`
       (which established the `SIZE_MAX` clamping pattern already applied to `buffer.c` and `vector.c`)

## 1. Purpose

The 2026-10-15 memory-optimization work clamped geometric-growth overflow in
`cobalt_buffer_ensure_writable` and `cobalt_vector` create/reserve — but the same
unguarded `capacity * 2` (or `capacity * 2 * sizeof(T)`) pattern is still scattered
through 11 other sites. This design finishes that sweep. While auditing those sites,
two additional deterministic defects surfaced; they are in scope too:

- **JSON DOM `\u` surrogate pairs** — `json_parse.c` emits *invalid UTF-8* for
  `0xD800-0xDFFF` and never synthesizes a 4-byte pair (the SAX parser already does).
- **eventloop signal counter** — `g_signal_count` is a plain `int` mutated inside an
  async-signal context (UB on any port).
- **json_sax initial-segment malloc** — `json_sax.c:285` NULL-checks nothing after
  `malloc`, so an `init_seg >= 4096` OOM NULL-derefs.

Direction chosen (of A/B/C considered): **A — complete the established pattern**,
the lowest-risk line that both finishes the committed hardening work and fixes two
real defects. C (fuzz harness in CI) is deferred to a follow-up round.

## 2. Scope

### In scope (14 change points)

| # | File:line (pre-edit) | Defect | Fix |
|---|---|---|---|
| 1 | `regex.c:233` | ast node pool `capacity*2` × `sizeof(ast_node_t)` can wrap | clamp to `SIZE_MAX / sizeof(ast_node_t)` |
| 2 | `regex.c:759` | codegen instr table `cap?cap*2:32` × sizeof | clamp to `SIZE_MAX / sizeof(cobalt_regex_inst_t)` |
| 3 | `treemap.c:520` | iterator stack `cap?cap*2:16` × sizeof(ptr) | clamp to `SIZE_MAX / sizeof(treemap_node_t *)` |
| 4 | `eventloop.c:344` | timer table `cap?cap*2:64` × sizeof | clamp to **largest power of two ≤ ceiling** (see §4.1) |
| 5 | `eventloop.c:756` | pollfds `int cap?cap*2:N`; int overflow + int×sizeof | clamp to `INT_MAX / sizeof(struct pollfd)` |
| 6 | `json_parse.c:200` | string grow `(len+elen+1)*2` | guard add-wrap, clamp `SIZE_MAX/2` |
| 7 | `json_parse.c:213` | string grow `capacity*=2` | clamp `SIZE_MAX/2` |
| 8 | `json_sax.c:284` | init-seg grow `(init_seg+1)*2` **+ missing malloc NULL check at :285** | clamp + NULL check (fail like existing OOM path) |
| 9 | `json_sax.c:379` | surrogate grow `(cap+ulen+1)*2` | guard add-wrap, clamp `SIZE_MAX/2` |
| 10 | `json_sax.c:405` | single-char grow `(cap+2)*2` | guard add-wrap, clamp `SIZE_MAX/2` |
| 11 | `json_sax.c:435` | fast-forward grow `(cap+seg_len+1)*2` | guard add-wrap, clamp `SIZE_MAX/2` |
| 12 | `json_parse.c:169-192` | DOM `\u` lone/lead surrogate → invalid 3-byte UTF-8 | surrogate-pair synthesis, §5 |
| 13 | `eventloop.c:157` | `g_signal_count` plain `int` in signal context | `volatile sig_atomic_t` |
| 14 | `eventloop.c:231/244/247` | matching read/write of the counter | type follows #13 |

### Out of scope

- Perf hot-path optimization (separate track, gated by the bench 5% regression CI).
- Arena `total_allocated` reset accounting (low-severity bookkeeping, no behavior impact).
- threadpool submit-vs-destroy contract (caller-misuse; needs API decision, not this sweep).
- libFuzzer harness / CI fuzz gate (direction C, follow-up round).
- `lfqueue` / `lfskiplist` / `deque` / `taskgraph` / `shm_channel` — audit-clean, no change.

## 3. Pattern (no new helper; inline at each site)

The repo already carries this exact shape in `vector.c:84-91` and `buffer.c:157-160`.
Each geometric-growth site adopts it inline, matching local style:

```c
/* sized-pool site (regex / treemap / eventloop tables) */
size_t max_cap = SIZE_MAX / sizeof(elem_t);
size_t new_cap = cap == 0 ? initial : (cap > max_cap / 2 ? max_cap : cap * 2);
```

Once clamped, the subsequent `realloc`/`alloc` of `new_cap * sizeof(elem_t)` cannot
overflow: `new_cap <= max_cap` ⟹ `new_cap * sizeof(elem_t) <= SIZE_MAX`. OOM falls
through each module's **existing** failure path with unchanged semantics:

| module | existing OOM path |
|---|---|
| regex (1,2) | `pool->oom = true` / `cg->oom = true` → `AST_NULL` / 0 |
| treemap (3) | `iter->finished = 1` |
| eventloop (4,5) | `return -1` |
| json_parse (6,7) | `jfree(result); return NULL` |
| json_sax (8-11) | `free(buf); return set_error(...)` |

## 4. Site specifics

### 4.1 eventloop timer table (site 4) — power-of-two constraint

`timer_table_insert` computes `idx = hash & (new_cap - 1)` at `:354` — the table is a
power-of-two hash table. A generic `SIZE_MAX/sizeof(T)` ceiling is **not** a power of
two, so the clamp target is pinned to a named power of two:

```c
/* Largest power of two that keeps `cap * sizeof(entry)` well under SIZE_MAX
 * on 64-bit and representable on 32-bit. 2^30 * 16 bytes (uint64_t + ptr) = 16 GiB,
 * far beyond any realistic timer table. */
#define COBALT_TIMER_TABLE_MAX_CAP (1u << 30)

size_t new_cap;
if (table->capacity == 0)
    new_cap = 64;
else if (table->capacity >= COBALT_TIMER_TABLE_MAX_CAP)
    new_cap = table->capacity; /* capped: retry same size; OOM -> -1 */
else
    new_cap = table->capacity * 2 > COBALT_TIMER_TABLE_MAX_CAP
                  ? COBALT_TIMER_TABLE_MAX_CAP
                  : table->capacity * 2;
```

Behavior at the cap: the `alloc` retries the *same* size; on OOM → `return -1`
(timer registration fails), never a smaller wrap. The load-factor rehash bound
(`count * 10 >= capacity * 7`, `:343`) still holds because capacity never shrinks.

### 4.2 eventloop pollfds (site 5) — int domain

`loop->pollfds_capacity` is `int` and the allocation is `sizeof(struct pollfd) *
new_cap` computed in the **int** domain. Clamp in int, then the int→size promotion is
safe:

```c
int max_cap = INT_MAX / sizeof(struct pollfd);
int new_cap;
if (loop->pollfds_capacity == 0)
    new_cap = COBALT_EVENTLOOP_DEFAULT_CAPACITY;
else if (loop->pollfds_capacity > max_cap / 2)
    new_cap = max_cap; /* capped: retry same size; OOM -> -1 */
else
    new_cap = loop->pollfds_capacity * 2;
```

### 4.3 json string growth (6,7,8-11) — addition-wrap guards

`(a + b + 1) * 2` can overflow even when `*2` alone wouldn't. Guard the sum first:

```c
size_t need = (len > SIZE_MAX - elen - 1) ? SIZE_MAX : len + elen + 1;
size_t cap2 = (need > SIZE_MAX / 2) ? SIZE_MAX : need * 2;
```

Sites 8-11 use the same guard with their respective `need` (405 uses `cap+2`, 435 uses
`cap+seg_len+1`, 379 uses `cap+ulen+1`).

Site 8 additionally wraps the `malloc` in a NULL check (currently bare at `:285`):

```c
if (init_seg >= cap) {
    cap = clamped_grow(init_seg + 1);
    buf = (char *)malloc(cap);
    if (!buf) return set_error(p, COBALT_JSON_ERR_DEPTH_EXCEEDED, "Out of memory");
    allocated = true;
}
```

## 5. JSON DOM surrogate-pair synthesis (site 12)

`json_parse.c` `json_parse_string`, the `\u` case, currently:

- `cp < 0x80` → 1 byte; `cp < 0x800` → 2 bytes; else → 3 bytes. With 4-hex, `cp`
  maxes at `0xFFFF`, so `0xD800-0xFFFF` all hit the 3-byte branch.
- **Defects:**
  - `0xD800-0xDFFF` → emits `ED A0-BF xx`, which is **invalid UTF-8** (surrogates have
    no valid 3-byte encoding).
  - A proper pair `\uD83D\uDE00` → two independent 3-byte writes instead of one
    4-byte `F0 9F 98 80`.

`escaped[5]` already has room for 4 bytes, so no buffer change is needed.

**New behavior (DOM mirrors SAX `json_sax.c:345-370`):**

```
hi in [0xD800,0xDBFF] and next 6 chars are "\u" + low in [0xDC00,0xDFFF]:
    cp = 0x10000 + ((hi-0xD800)<<10 | (low-0xDC00))
    emit 4-byte UTF-8; consume the second \uXXXX
hi in [0xD800,0xDBFF] with no valid low pair, OR low in [0xDC00,0xDFFF] lone:
    → json_parse_string returns NULL
```

On lone-surrogate failure, `json_parse_string` returns NULL, which flows through the
DOM's **existing recoverable-error channel**: in object/array parsing a `NULL` value
aborts the loop and returns a **partial tree** (documented in
`test_json_malformed_paths`); at the top level a lone-surrogate string returns NULL.
This is the "fail + partial tree" semantic chosen in review — consistent with the OOM
path and with the DOM's partial-tree convention, and close in spirit to SAX's stricter
"Invalid UTF-16 surrogate pair" error.

## 6. Signal counter (sites 13-14)

`g_signal_count` becomes `volatile sig_atomic_t`. The handler (`:231`) does `count++`
only after the free-slot check passes (no loss/distortion when the ring is full —
the handler still checks `next != head` first). The drain loop (`:244/247`) does
`count > 0` / `count--`. No behavioral change, no new tests: the existing eventloop
signal tests passing is the regression proof; the change removes async-signal UB.

## 7. Testing

Repo rule: tests must catch a real consumer-visible defect, not re-pin plumbing.

- **Sites 1-11 (clamps):** the wrap is only reachable near `SIZE_MAX/2` via *internal*
  growth, not through a public entry point (unlike `vector` create/reserve, which
  accept a caller-supplied `SIZE_MAX`). Faking those inputs is forbidden, so these
  carry **no new dedicated tests** — the full suite staying green (plus ASan) is the
  regression net. This is stated honestly in the changelog, not papered over.
- **Site 12 (DOM surrogate) — real, testable behavior**, add to `tests/unit/test_json.c`:
  - `"​\uD83D\uDE00"` (Grinning Face U+1F600) → byte-exact `F0 9F 98 80` in the parsed string.
  - object key `"​\uD800"` (lone lead, no pair) → partial tree: earlier members present,
    later members absent, root non-NULL.
  - top-level string `"​\uD800"` → `json_parse` returns NULL.
  - mirrored lone-low `"​\uDE00"` → same NULL/partial behavior.
- **Sites 13-14 (sig_atomic_t):** type-only; existing `test_eventloop` signal tests
  are the regression proof. No new test.
- **Site 8 malloc NULL check:** not unit-testable deterministically (would need
  forced OOM); covered by the ASan build running the JSON filters.

## 8. Verification & commit

- Debug full build: zero warnings.
- `ctest` full: all green, including the new `test_json` subtests and existing
  `test_eventloop` / `test_regex` / `test_treemap` / `test_json_sax` filters.
- ASan build: run `json`, `json_sax`, `regex`, `eventloop` filters clean.
- Commits (gitmoji, per `AGENTS.md`):
  - `fix: 🐛 clamp geometric-growth overflow in regex, treemap, eventloop timer table and json parsers`
  - `fix: 🐛 synthesize JSON DOM UTF-16 surrogate pairs, guard json_sax malloc, make eventloop signal counter sig_atomic_t`
- `CHANGELOG.md` Unreleased: one entry per commit above; note explicitly that the
  11 clamp sites are defensive (no new dedicated tests, suite + ASan is the net).
