# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 2.4.x   | ✅                 |
| 2.3.x   | ⚠️  security fixes only |
| < 2.0   | ❌                 |

## Reporting a Vulnerability

Cobalt is a zero-dependency C11 framework. Security concerns are taken seriously.

**Please do NOT open a public issue for security vulnerabilities.**

Instead, report privately:

1. **Email**: [security@example.com](mailto:security@example.com)
2. **GitHub**: Use the [Security Advisory](https://github.com/quintinliu/cobalt/security/advisories/new) page

### What to include

- Description of the vulnerability
- Steps to reproduce
- Potential impact (memory safety, information disclosure, DoS)
- Suggested fix (if any)

### What to expect

- **Acknowledgment** within 48 hours
- **Assessment** within 1 week
- **Fix target** communicated within 2 weeks for confirmed vulnerabilities
- **Credit** in release notes (unless you prefer anonymity)

## Security Scope

This project follows responsible disclosure for:

- **Memory safety**: Buffer overflows, use-after-free, double-free, heap corruption
- **Cryptographic**: Incorrect random number generation, weak defaults
- **API safety**: Null pointer dereferences, integer overflows, unchecked inputs
- **Build security**: Supply chain, build script integrity

## Current Security Posture

### Measures in place

- **ASan/UBSan**: CI runs AddressSanitizer and UndefinedBehaviorSanitizer on every PR
- **Valgrind**: Leak detection via `valgrind --leak-check=full` in CI
- **Coverage**: Minimum 80% branch coverage enforced (`cmake check_coverage`)
- **Static analysis**: clang-tidy with bugprone/performance/readability checks
- **Formatter gate**: clang-format ensures consistent code style
- **ABI regression gate**: Symbol diff detects breaking changes
- **Version script**: `cobalt.map` controls symbol export surface (317 symbols)
- **Custom allocators**: Injection points throughout object system to prevent raw malloc leaks

### Known limitations

- **thread.c**: `cobalt_mutex_t`, `cobalt_cond_t`, `cobalt_thread_t` use system `malloc` directly. These are thin wrappers around POSIX primitives and are singleton-per-thread — allocator injection provides no security benefit.
- **allocator.c**: The system allocator implementation itself uses `malloc`/`free` by definition.
- **benchmark.c**: Test helper with raw allocations; not part of library API.

## Security Best Practices for Users

1. **Always pair create/destroy with the same allocator**
   ```c
   cobalt_allocator_t *alloc = cobalt_allocator_get_system();
   cobalt_vector_t *v = cobalt_vector_create_with_allocator(sizeof(int), 4, alloc);
   // ... use v ...
   cobalt_vector_destroy_with_alloc(v, alloc);  // must use same alloc
   ```

2. **Validate input sizes** to prevent integer overflow in allocation calculations

3. **Use thread-safe wrappers** for concurrent access:
   ```c
   cobalt_tsvector_t *tsv = cobalt_tsvector_create(...);
   ```

4. **Enable debug builds** in development for assertion coverage:
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Debug
   ```

5. **Run sanitizers** before deployment:
   ```bash
   cmake -DCMAKE_C_FLAGS="-fsanitize=address" && ctest
   ```

## History

| Date       | Version | Description                              |
| ---------- | ------- | ---------------------------------------- |
| 2026-09-05 | 2.4.0   | Initial security policy                  |
| 2026-08-08 | 2.3.0   | Fixed heap-buffer-overflow in cobalt_split (CWE-122) |
