# Cobalt Phase 1: Performance Internals Optimization

## Overview

Phase 1 focuses on internal performance improvements with no public API breaking changes. All changes are backward-compatible from the consumer's perspective. The goal is to fix correctness issues and improve algorithmic complexity in the memory allocator, hash map, and event loop subsystems.

## Scope

- Arena allocator alignment correctness
- HashMap resize policy and hash function quality
- Event loop timer data structure replacement
- Memory allocation strategy consistency
- Error handling documentation

## Design

### 1. Arena Allocator Alignment

**Problem:** `cobalt_arena_alloc()` returns memory that may not satisfy platform alignment requirements. On ARM64 and other strict-alignment architectures, storing `double`, `long long`, or SIMD types in unaligned memory is undefined behavior.

**Current behavior:**
- Allocates exact byte count requested
- No alignment guarantee beyond `malloc`

**Design:**
- Round up requested size to `alignof(max_align_t)` boundary (typically 8 or 16 bytes)
- Return pointer aligned to at least `alignof(void*)`
- Store original unrounded size internally to track actual usage accurately
- API signature unchanged: `void* cobalt_arena_alloc(cobalt_arena_t* arena, size_t size)`
- Internal bookkeeping uses `arena->used += round_up(size)` while returning aligned pointer

**Trade-offs:**
- Slightly higher memory usage (up to alignment padding per allocation)
- Eliminates undefined behavior on strict-alignment platforms

### 2. HashMap Resize & Load Factor

**Problem:**
- Current implementation never resizes buckets. As entries grow, load factor approaches 1.0, degrading lookup to O(n).
- `hash_string()` uses a simple shift-add loop with poor avalanche behavior, causing clustering.

**Current behavior:**
- Fixed bucket count set at creation time
- Linear probing via linked lists with weak hash distribution

**Design:**
- Add internal `max_load_factor` threshold (0.75, non-configurable in Phase 1)
- Track `impl.size / impl.bucket_count` on every insert
- When threshold exceeded, allocate new bucket array with 2× capacity and rehash all entries
- Replace `hash_string()` with FNV-1a 32-bit variant:
  ```
  h = 2166136261
  for each byte: h = (h ^ byte) * 16777619
  ```
- Preserve all existing API signatures; resize is fully internal

**Trade-offs:**
- Insert cost amortized: rare O(n) resize vs frequent O(1) average insert
- Better distribution reduces average chain length from ~load_factor to closer to 1.0
- Slightly more complex destroy path (already handles dynamic bucket count)

### 3. Event Loop Timer Heap

**Problem:**
- Timers stored in unsorted singly-linked list
- `add_timer_to_list()` performs O(n) insertion to maintain sorted order
- `process_expired_timers()` must scan entire list each iteration
- Complexity: O(n²) with n timers, unacceptable for event loops with hundreds/thousands of timers

**Current behavior:**
- Linked list with `next_fire` sorted insertion
- Timer deletion is O(n) search

**Design:**
- Replace `timer_entry_t* timer_head` with a binary min-heap
- Heap ordered by `next_fire` timespec
- Operations:
  - `push`: O(log n) heap insert
  - `pop_min`: O(log n) heap extract
  - `peek`: O(1) read root
  - `remove_by_id`: O(n) linear scan acceptable (timers removed infrequently relative to ticks)
- Maintain `timer_count` for bounds checking
- Keep linked list for `fd_entry_t` (FD count typically small, no performance issue)

**Trade-offs:**
- Adds ~1KB heap metadata for timer array (negligible)
- Dramatically improves scalability for timer-heavy workloads
- `remove_by_id` remains O(n) but is rare; can optimize in Phase 2 if needed

### 4. Memory Allocation Strategy Consistency

**Problem:**
- `cobalt_vector_create(0)` allocates zero-capacity array; first push triggers growth
- `cobalt_hashmap_create(0)` silently defaults to 16 buckets
- Inconsistent zero-initialization behavior across containers

**Design:**
- Document standard: zero initial capacity means "allocate on first use"
- Change `cobalt_hashmap_create(0)` to allocate empty bucket array (calloc(0) is implementation-defined, so allocate 1 bucket minimum or use NULL + lazy init)
- Preferred approach: accept 0 as valid, treat as "will grow on first insert"
- Implementation: if `initial_buckets == 0`, set `bucket_count = 0`, `buckets = NULL`, and handle in `put()` by lazy-allocating minimum 16 buckets when first entry inserted
- No API signature changes

**Trade-offs:**
- First `put()` slightly slower due to lazy allocation
- Consistent semantic across all containers

### 5. Error Handling Consistency

**Problem:**
- Mixed return conventions: `NULL`, `-1`, `0` success, no error codes
- Consumers cannot distinguish "out of memory" from "invalid argument" without reading source

**Design:**
- Add `cobalt_errno_t` enum to `error.h`:
  ```
  COBALT_OK = 0
  COBALT_ERR_INVALID_ARG = -1
  COBALT_ERR_OUT_OF_MEMORY = -2
  COBALT_ERR_NOT_FOUND = -3
  COBALT_ERR_ALREADY_EXISTS = -4
  COBALT_ERR_IO = -5
  COBALT_ERR_UNSUPPORTED = -6
  ```
- Document that existing return values (`NULL`, `-1`) map to `COBALT_ERR_INVALID_ARG` or `COBALT_ERR_OUT_OF_MEMORY` as appropriate
- Do NOT change existing function signatures in Phase 1
- Add `cobalt_errno()` getter for thread-local last error
- Update internal implementations to set thread-local errno on failure paths

**Trade-offs:**
- Zero breaking changes
- Enables richer error handling in Phase 2 API additions

## Testing Strategy

Each change requires targeted unit tests:

1. **Arena alignment**: Test allocations of sizes 1, 3, 7, 8, 15, 16, 33 and verify returned pointer alignment with `_Alignas` or `uintptr_t % align == 0`
2. **HashMap resize**: Insert N entries where N > 0.75 * initial_buckets and verify lookup correctness after resize; test with adversarial keys to verify hash distribution
3. **Timer heap**: Create 1000+ timers, verify all fire in correct order; verify cancel/modify operations
4. **Allocation consistency**: Create vector and hashmap with 0 initial capacity, verify they grow correctly on first use
5. **Error handling**: Trigger each error path and verify `cobalt_errno()` returns expected code

## Rollout Plan

- All changes committed to `feature/phase1-performance` branch
- Each subsystem change in its own commit with focused test additions
- Merge to `master` after full test suite passes (`ctest --output-on-failure`)

## Out of Scope for Phase 1

- Public API signature changes
- New containers or modules
- Windows IOCP implementation
- CI/CD pipeline improvements
- Documentation rewrites beyond inline comments
