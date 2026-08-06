# API Stability Policy

## Versioning

Cobalt follows [Semantic Versioning](https://semver.org/) (MAJOR.MINOR.PATCH).

| Version Bump | Meaning |
|---|---|
| MAJOR (3.0.0) | Breaking API changes; ABI may change |
| MINOR (2.1.0) | New features added in a backward-compatible manner |
| PATCH (2.0.1) | Bug fixes; no API changes |

## Stability Guarantees

### Stable API (L3–L8, containers, algorithms, runtime)

- Function signatures for public headers under `include/cobalt/` are stable within a MAJOR version.
- New functions may be added; existing functions will not have their signatures changed.
- Error codes in `cobalt_error_t` may gain new values but existing values are unchanged.

### Extension APIs (marked `_ext`)

- Functions suffixed with `_ext` (e.g., `cobalt_hashmap_create_ext`) are extension APIs.
- They may change or be removed in a MINOR release without a MAJOR bump.
- Use them at your own risk in production-critical code.

### Opaque Types

- All container types (`cobalt_vector_t`, `cobalt_hashmap_t`, etc.) are opaque handles.
- Internal fields must not be accessed directly.
- Use the provided getter/setter/iterator APIs.

### Module-level APIs (L2: JSON, EventLoop)

- JSON parsing/serialization API is stable but the internal `json_node_t` representation is opaque.
- Event loop API (`cobalt_eventloop_*`) is stable within a MAJOR version.

## What is NOT Covered

- Example programs and benchmarks are not part of the public API.
- Header-only utility macros (e.g., `cobalt_foreach`) may change between MINOR versions.
- Build system files (`CMakeLists.txt`, `*.pc.in`) are for library consumers and may evolve.

## Migration Guide

When upgrading between MAJOR versions, consult `CHANGELOG.md` for breaking change notes.
