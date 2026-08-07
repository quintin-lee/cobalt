# API Reference

Generated API reference documentation for the Cobalt C Framework.

## Structure

| File | Coverage |
|------|----------|
| [cobalt.h.md](cobalt.h.md) | Core Object (L6), Memory/Runtime (L7), Platform (L8) |
| [containers.md](containers.md) | All 8 containers (L4): Vector, List, Stack, Queue, Deque, HashMap, TreeMap, Set |
| [interfaces.md](interfaces.md) | Map, Sequence, and Iterator protocols |
| [core.md](core.md) | Object system, class, and interface |
| [memory.md](memory.md) | Allocator and arena |
| [runtime.md](runtime.md) | Error handling and logging |
| [platform.md](platform.md) | Platform detection and atomic ops |
| [algorithm.md](algorithm.md) | Sorting, predicates, search, and stream operators (L3) |
| [modules.md](modules.md) | JSON and Event Loop (L1-2) |

## Quick Reference

- **Containers**: See [containers.md](containers.md) for O(1)/O(log n) complexity tables
- **Algorithms**: See [algorithm.md](algorithm.md) for `map`/`filter`/`fold` stream operators
- **Objects**: See [core.md](core.md) for ref-count lifecycle and class system
- **Modules**: See [modules.md](modules.md) for JSON parsing and event loop APIs

## Building

```bash
cd build && cmake .. && cmake --build . --parallel && ctest --output-on-failure
```
