# Cobalt Framework

Cobalt is a comprehensive C framework providing modern programming features for C11 projects. It includes:

- **Core System**: Object-oriented class system with RTTI and reference counting
- **Memory Management**: Arena-based and general-purpose allocators
- **Containers**: Vector, list, hashmap, treemap implementing sequence/map interfaces
- **Algorithms**: Sorting and generic functional utilities
- **Platform Abstraction**: Cross-platform detection and handles
- **Event Loop**: Asynchronous I/O loop with timer support
- **JSON**: Parsing and serialization module
- **Logging**: Thread-safe logging facility

## Structure

```
docs/           Documentation
├── API/        Public API reference
├── SPEC/       Module specifications
├── RFC/        Request for comments (design decisions)
└── ...

include/        Public headers
└── cobalt/     All public APIs (cobalt.h)

src/            Implementation
├── core/       Object/class system
├── container/  Data structures
├── algorithm/  Algorithms
└── ...

cmake/          Build configuration
```

## Building

Cobalt uses CMake for building:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
make test
```

## Project Standards

This project follows the [C Project Standards](c-project-standards.md):
- K&R brace style
- 4-space indentation
- snake_case naming
- Include guards on all headers
- Memory management requires free() after malloc()

