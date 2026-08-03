# Cobalt C Framework

A lightweight, zero-dependency C11 framework providing object-oriented capabilities, memory management, and cross-platform abstractions.

## Features

- **8-Layer Architecture**: Clean separation of concerns
- **Object System**: Single inheritance + multi-interface support
- **Memory Management**: Arena allocator with automatic cleanup
- **Thread Safety**: Atomic operations with stdatomic.h
- **Containers**: Vector, List, HashMap, TreeMap, Stack, Queue, Set, Deque
- **Algorithms**: Sort, binary search, find_if, for_each, predicates
- **Modules**: JSON parser/serializer, event loop (epoll/kqueue)
- **Cross-platform**: Linux, macOS, Windows support

## Architecture

```
L8  Platform       → Platform detection, atomic operations
L7a Memory         → System allocator, Arena allocator
L7b Runtime        → Error handling, Structured logging
L6  Core           → Object system, Class system, Interface
L5  Interface      → Sequence, Map, Iterator protocols
L4  Container      → Vector, List, HashMap, TreeMap, Stack, Queue, Set, Deque
L3  Algorithm      → Sort algorithms, Predicate functions
L2  Module         → JSON parser/serializer, Event loop
L1  Application    → Example applications
```

## Building

```bash
# Configure and build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# Run tests
ctest --output-on-failure

# Install (optional)
make install DESTDIR=/tmp/stage
```

## Quick Start

```c
#include <cobalt/cobalt.h>

int main() {
    // Platform detection
    cobalt_platform_id_t platform = cobalt_platform_get_id();
    
    // Create a vector
    cobalt_vector_t *vec = cobalt_vector_create(10);
    int value = 42;
    cobalt_vector_push(vec, &value);
    
    // Create a hashmap
    cobalt_hashmap_t *map = cobalt_hashmap_create(16);
    cobalt_hashmap_put(map, "key", &value);
    
    // Cleanup
    cobalt_vector_destroy(vec);
    cobalt_hashmap_destroy(map);
    
    return 0;
}
```

### Building with pkg-config

```bash
# After install
pkg-config --cflags cobalt
pkg-config --libs cobalt

# Compile
gcc main.c $(pkg-config --cflags --libs cobalt) -o main
```

### Building with CMake (find_package)

```cmake
find_package(Cobalt REQUIRED)
add_executable(myapp main.c)
target_link_libraries(myapp Cobalt::cobalt)
```

## Testing

```bash
cd build
ctest --output-on-failure  # 22 tests passing
# or with verbose output
ctest --verbose
```

## Examples

The project includes 14 runnable examples covering all major features:

| Example | Description | Layer |
|---------|-------------|-------|
| `basic_usage` | Platform detection, allocator, arena, logger, vector, hashmap | L7a-L4 |
| `hello_world` | Logging initialization and basic API usage | L7b |
| `containers_demo` | All container types (Vector, List, HashMap, TreeMap, Stack, Queue, Set, Deque) | L4 |
| `vector_demo` | Vector operations: create, push, get, set, destroy | L4 |
| `hashmap_demo` | HashMap operations: put, get, remove, iterate | L4 |
| `arena_allocator` | Arena allocator lifecycle: alloc, reset, destroy | L7a |
| `class_hierarchy` | Object-oriented class inheritance and reference counting | L6 |
| `refcount_demo` | Thread-safe reference counting with atomic operations | L8 |
| `sort_example` | Sorting algorithms: qsort and insertion sort | L3 |
| `json_example` | JSON parsing, navigation, serialization | L2 |
| `simple_server` | Event loop with timer callbacks | L2 |
| `object_oriented` | Object creation, ref/unref lifecycle | L6 |
| `memory_management` | Arena allocation patterns | L7a |
| `advanced_usage` | Atomic operations, JSON, Event loop integration | L2-L8 |

### Running examples

```bash
cd build
./examples/basic_usage
./examples/containers_demo
./examples/hello_world
# ... etc
```

## Documentation

- [RFC](docs/RFC/) - Architecture decisions
- [SPEC](docs/SPEC/) - Module specifications
- [API](docs/API/) - Public API reference
- [Examples](docs/EXAMPLES/) - Usage examples
- [Tutorial](docs/TUTORIAL/) - Getting started guide
- [Developer Guide](docs/DEV_GUIDE/) - Coding standards and workflow

## Project Statistics

| Category | Count |
|----------|-------|
| Source files (.c) | 25 |
| Header files (.h) | 26 |
| Test files (.c) | 22 |
| Example files (.c) | 14 |
| Documentation (.md) | 27 |

| Metric | Value |
|--------|-------|
| Total lines of code | 6,627 |
| Git commits | 163 |
| Tests passing | 22/22 |

## License

MIT License - See [LICENSE](LICENSE) for details.
