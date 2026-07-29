# Developer Guide for Cobalt Framework

This guide describes the coding conventions, development workflow, and contribution processes for the Cobalt C framework project.

## 1. Coding Standards

### 1.1 File Structure

All source files must follow this header template:

```c
/**
 * @file filename.c
 * @brief Short description of file purpose
 *
 * Longer description of responsibilities, design rationale,
 * and any special considerations. This section is optional.
 */

/* Include guards (for .h files) or inclusions (for .c files) */
#include "header1.h"
#include "header2.h"

/* Implementation follows... */
```

Include guard naming convention: `FILENAME_H` where FILENAME is the header file name in uppercase with `_` replacing `/` and other non-alphanumeric characters. Example: `include/cobalt/core/object.h` → `OBJECT_H`.

### 1.2 Formatting

- **Indentation**: 4 spaces per level, no tabs
- **Line width**: Keep under 80 characters where possible; wraps at logical boundaries
- **Braces**: K&R style (opening brace on same line as statement), except for function definitions which use separate line for opening brace when preceded by a declaration block
- **Spacing**: One space after keywords (`if`, `while`, `for`); one space around binary operators (`=`, `+`, `-`, `*`, `/`, `==`, `!=`)
- **Trailing whitespace**: Must be trimmed automatically before commit
- **Header comments**: Every public function/class/struct requires a brief Doxygen-style comment above its declaration

### 1.3 Naming Conventions

- **Functions/variables**: `lower_case_with_underscores` (snake_case)
- **Constants/macros**: `UPPER_CASE_WITH_UNDERSCORES` (all caps)
- **Structs/typedefs**: `PascalCase` or `snake_case_t` (Pascal preferred for clarity)
- **Header guards**: `MODULE_NAME_H` all uppercase
- **Private/internal symbols**: Prefix with underscore (`_internal_func`) or place in internal subdirectories not exposed via public includes

### 1.4 Memory Management Rules

- Every call to `malloc`/`calloc`/`realloc` must have a corresponding `free` in all code paths (including error returns)
- Use arena/pool allocation where appropriate to reduce fragmentation and improve performance
- All object lifetimes are managed via explicit reference counting (`cobalt_object_ref` / `cobalt_object_unref`)
- Never access memory after it has been freed; set pointers to NULL after freeing to catch double-free bugs early during development

### 1.5 Error Handling

- Return `cobalt_error_t` codes from functions that may fail
- Provide an optional error-out parameter for detailed diagnostics: `cobalt_error_t *err`
- Log errors using the logging macros (`cobalt_error()`, `cobatal_fatals()` etc.) with context information (file, line number)
- Do not swallow errors; propagate them up the call chain until handled appropriately

### 1.6 Thread Safety Guarantees

Document thread-safety guarantees for every public API in comments. Examples:

```c
/// This function is thread-safe: concurrent calls from multiple threads
/// do not require external synchronization.
cobalt_error_t cobalt_platform_get_id(void);

/// Not thread-safe: must be serialized during initialization phase.
cobalt_class_t *cobalt_class_create(const char *name, cobalt_class_t *base_class);
```

## 2. Build System Usage

### 2.1 Configuration Options

CMake provides several configuration options:

```bash
# Debug build with assertions and full debug info
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build optimized for size
cmake .. -DCMAKE_BUILD_TYPE=Release -DCOBALT_OPTIMIZE_SIZE=ON

# Enable unit tests (default ON)
cmake .. -DBUILD_TESTING=ON

# Install prefix
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
```

### 2.2 Building and Testing

```bash
mkdir build && cd build
cmake .. --preset # or specify manually
make            # builds library + targets
make test       # runs CTest suite
make install    # installs to prefix (optional)
make clean      # cleans build artifacts
```

### 2.3 Generating Compile Commands

For IDE integration, ensure compile_commands.json is generated:

```bash
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

The `.clangd` config already points to `build/compile_commands.json`.

## 3. Contribution Process

### 3.1 Forking and Cloning

1. Fork the repository on GitHub/GitLab
2. Clone your fork locally:

```bash
git clone git@github.com:<username>/cobalt.git
cd cobalt
git remote add upstream git://github.com/cobalt-project/cobalt.git
```

### 3.2 Branching Model

Use feature branch naming convention:

```bash
# For new features:
git checkout -b feat/<feature-name>-<description>

# For bug fixes:
git checkout -b fix/<short-description>

# For documentation updates:
git checkout -b docs/<topic>
```

Example: `feat/json-parser-support`, `fix/vector-bounds-check`, `docs/api-reference-update`

### 3.3 Commit Message Format

Follow [gitmoji](https://gitmoji.dev/) convention:

```
[<type>]<scope>: [<emoji>] <subject>
```

**Types:**
- `feat` ✨ — new feature
- `fix` 🐛 — bug fix
- `docs` 📝 — documentation only
- `style` 🎨 — formatting (no functional change)
- `refactor` ♻️ — code restructuring (no functional change)
- `test` ✅ — adding/fixed tests
- `build` 📦 — build system changes
- `ci` 👷 — CI/CD changes
- `chore` 🧹 — tooling/config changes (non-src/test)

**Scope** indicates affected subsystem (e.g., `(core)`, `(docs)`, `(memory)`). If unclear, use `(core)`.

Example: `feat(core): ✨ implement arena allocator for cobalt_arena_create()`

### 3.4 Pull Request Requirements

Before submitting PR:

1. All new code must follow the coding standards documented herein
2. New functionality must include corresponding tests (see Testing section)
3. Documentation must be updated if API surface changes
4. No linting/warning errors should be introduced (ensure `-Werror` passes)
5. All existing tests must continue to pass

PR title should match commit message format above. PR description should summarize changes and link any related issues/RFCs.

## 4. Testing Strategy

### 4.1 Unit Tests

Unit tests reside in a `tests/` directory (to be created). Each test corresponds to a specific module/functionality. Use the CUnit or custom lightweight test framework integrated into CMake.

Example test skeleton:

```c
#include <assert.h>
#include "unittest.h"  /* hypothetical macro library */

TEST_FUNC(test_vector_push_empty) {
    cobalt_vector_t *vec = cobalt_vector_create(0);
    assert(vec != NULL);
    
    int value = 42;
    int ret = cobalt_vector_push(vec, &value);
    assert(ret == 0);
    assert(cobalt_vector_size(vec) == 1);
    
    cobalt_vector_destroy(vec);
}
```

Run tests with: `make test` or `ctest --output-on-failure`

### 4.2 Integration Tests

Higher-level tests that exercise combinations of modules (e.g., JSON parsing into containers, event loop callbacks invoking algorithms). These validate the overall architecture behaves correctly under realistic usage patterns.

### 4.3 Performance Benchmarks (Future)

Optional benchmarks measuring latency/memory overhead of critical paths (allocator performance, map lookup time, sorting speed). Integrated into CI via custom benchmark target.

## 5. Code Review Checklist

When reviewing PRs, verify:

- [ ] Code follows formatting/naming conventions in this document
- [ ] All new public functions have Doxygen-style comments
- [ ] No memory leaks detected (check allocations/frees balance)
- [ ] Thread safety claims are accurate and documented
- [ ] Error handling covers all failure paths including OOM
- [ ] Tests cover edge cases (NULL inputs, empty collections, boundary conditions)
- [ ] No accidental commits of IDE-specific files (.idea/, .vscode/), binaries, or large assets
- [ ] Commit messages are clear and follow gitmoji convention
- [ ] Bumping of version numbers happens ONLY in release branch/tag (not in feature branches)

## 6. Versioning Policy (Semantic Versioning)

Cobalt uses Semantic Versioning (SemVer) `MAJOR.MINOR.PATCH`:

- **MAJOR** breaking changes (API incompatibilities, architectural shifts)
- **MINOR** backward-compatible additions (new functions, optional parameters)
- **PATCH** backward-compatible bug fixes only (no new features)

Deprecation cycle: mark APIs as deprecated with `DEPRECATED` annotation in headers; remove after two minor releases with appropriate deprecation warnings in changelog.

## 7. License and Copyright

All contributions must comply with the project's chosen open-source license (typically MIT or Apache-2.0; check LICENSE file). Contributors retain copyright but grant appropriate licensing rights as per the contributor license agreement (CLA) if required by the project maintainers.

---

*Last updated: Cobalt v2.0.0 Baseline*
