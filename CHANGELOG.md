# Cobalt Change Log

## v2.3.0 (2026-08-08) — Security Fixes & Advanced APIs Release

### Added
- **HashMap runtime hash/equal replacement**: `cobalt_hashmap_set_funcs()` for swapping hash and equality callbacks on an existing instance
- **TreeMap custom comparator**: `cobalt_treemap_create_ext()` with `cobalt_compare_func_t` for generic key comparison (not just strings)
- **Eventloop UNIX domain sockets**: `cobalt_eventloop_create_unix_server()` and `cobalt_eventloop_accept()` for local IPC
- **String utilities**: `cobalt_split()`, `cobalt_join()`, `cobalt_strip()` in `utils/string.h`
- **ASan suppressions**: Added `lsan.suppress` for known allocator-inject test infrastructure leaks

### Fixed
- **Heap-buffer-overflow in `cobalt_split()`**: Fixed allocation size miscalculation (operator precedence bug in `malloc`)
- **Memory leak in `test_thread_safety`**: Added proper cleanup of hashmap-allocated values
- **Double-free in `test_thread_safety`**: Removed premature `free(got)` before loop cleanup
- **CI shell line continuations**: Fixed `\\` → `\` in `.github/workflows/ci.yml` causing `command not found` errors
- **CI benchmark job**: Added `mkdir -p .benchmarks` before running benchmarks

### Changed
- HashMap now supports `cobalt_hashmap_create_ext()` for allocator-compatible creation
- TreeMap uses `rb_compare()` that delegates to configurable comparator
- Eventloop epoll dispatch fixed: `data.ptr` now correctly set instead of `data.fd`
- Updated test count: 25 → 41 (added thread safety, allocator inject, string tests)

### Security
- Fixed CWE-122 (heap buffer overflow) in `cobalt_split()` — allocation was `sizeof(char*)*cnt + 1` instead of `sizeof(char*)*(cnt+1)`

This project follows [Semantic Versioning](https://semver.org/) (MAJOR.MINOR.PATCH).

---

## v2.2.0 (2026-08-07) — Interface & Documentation Release

### Added
- **Sequence convenience API**: `cobalt_sequence_size`, `cobalt_sequence_is_empty`, `cobalt_sequence_add`, `cobalt_sequence_remove`
- **Deque implements `cobalt_sequence_t`**: polymorphic sequence usage for Deque
- **`docs/API/`**: Added core.md, memory.md, interfaces.md, runtime.md, platform.md
- **CI hardening**: Added format gate, ASan job, and Valgrind memory-check job

### Changed
- Moved `find_program` and `ALL_SRC_FILES` to top of CMakeLists.txt (fixes POST_BUILD format hang)
- Updated test count: 22 → 25

### Fixed
- fix(build): POST_BUILD format no longer hangs when variables are undefined
- style(container): fix readability-else-after-return in deque.c

---
## v2.1.0 (2026-08-02) — Feature Expansion Release

### Added
- **Set container**: Hash-based set with insert, remove, contains operations
- **Deque container**: Double-ended queue with push_front/back, pop_front/back
- **New algorithms**: binary search (cobalt_bsearch), find_if, for_each
- **JSON module split**: Separated json.c into json_parse.c and json_serialize.c
- **Shared string utility**: Consolidated my_strdup into cobalt_strdup()

### Changed
- Updated architecture documentation to reflect new containers and algorithms
- Updated code statistics (53 source/header files, 6627 lines, 83 commits)
- Updated test count: 22 tests passing

### Fixed
- Fixed duplicate compare_func_t typedef (now only in sort.h)
- Fixed header include order in cobalt.h

## v2.0.0 (2026-07-29) — Architecture Baseline Release

### Changed
- Complete 8-layer architecture implementation (L8 Platform → L1 Applications)
- Added core object system with single inheritance + multi-interface support
- Implemented container hierarchy: Sequence → Vector/List → Map → HashMap/Treemap
- Added algorithm layer with sorting and functional utilities
- Added JSON module and event loop module
- Created comprehensive documentation suite (RFC, SPEC, API, UML, DEV_GUIDE, EXAMPLES, TUTORIAL)

### Added
- `.gitignore` with AI tooling caches exclusion
- CMake build system with pkg-config and find_package support
- `.clangd` configuration for IDE integration
- All header files with include guards
- All source implementations (.c files)
- Specification documents for all 8 layers
- Developer guidelines and contribution workflow

### Fixed
- No bugs in initial baseline release

## v1.0.0 (TBD) — Initial Proposal

### Added
- Initial RFC document proposing the 8-layer architecture concept
- Foundation for object system design discussions

*Note: This is a development branch; public releases will be tagged separately.*
