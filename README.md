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
- **String utilities**: Split, join, and strip with `cobalt_split()`, `cobalt_join()`, `cobalt_strip()`
- **HashMap runtime replacement**: Swap hash/equal functions at runtime via `cobalt_hashmap_set_funcs()`
- **TreeMap custom comparator**: Generic key comparison with `cobalt_treemap_create_ext()`
- **UNIX domain sockets**: Local IPC with `cobalt_eventloop_create_unix_server()` and `cobalt_eventloop_accept()`
- **Security fix**: Heap-buffer-overflow patched in `cobalt_split()` (CWE-122)

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
ctest --output-on-failure  # 41/41 tests passing
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
| Source files (.c) | 33 |
| Header files (.h) | 35 |
| Test files (.c) | 34 |
| Benchmark files (.c) | 8 |
| Example files (.c) | 17 |
| Documentation (.md) | 41 |

| Metric | Value |
|--------|-------|
| Total lines of code | 10,982 |
| Git commits | 226 |
| 27/27 | 27/27 |


## CI/CD

Continuous integration runs on every push to `master`:

| Job | Description |
|-----|-------------|
| **build** | Build with GCC/Clang × Debug/Release (4 variants) with format + tidy gates |
| **asan** | AddressSanitizer build with LSan suppressions for allocator-inject tests |
| **ubsan** | UndefinedBehaviorSanitizer build |
| **valgrind** | Memory leak and error checking with Valgrind |
| **bench** | Performance benchmark regression detection |
| **coverage** | Code coverage reporting with lcov |
| **abi** | ABI snapshot comparison for breakage detection |

All jobs require passing format and clang-tidy checks before tests run.

## License


MIT License - See [LICENSE](LICENSE) for details.

## Package Managers

Available via popular package managers:

| Manager | Command |
|---------|---------|
| vcpkg | `vcpkg install cobalt` |
| Conan | `conan install --requires=cobalt/2.4.0` |
| pkg-config | `pkg-config --cflags --libs cobalt` |

## Performance

Benchmark results (Apple M2, clang 15, Release build, 100k operations, 3 iterations averaged):

| Operation | Time | Notes |
|-----------|------|-------|
| Vector push (100k) | ~1.4 ms | Contiguous memory, cache-friendly |
| Vector get (100k) | ~0.9 ms | O(1) random access |
| HashMap put (100k) | ~140 ms | String key hashing overhead |
| HashMap get (100k) | ~232 ms | Hash + pointer chase |
| TreeMap put (100k) | ~321 ms | BST traversal, cache misses |
| TreeMap get (100k) | ~460 ms | O(log n) lookup |
| List push_back (100k) | ~17.8 ms | Linked list allocation overhead |
| Deque push_front (100k) | ~17.4 ms | Efficient front insertion |
| Stack push (100k) | ~17.5 ms | LIFO, minimal overhead |
| Queue enqueue (100k) | ~15.0 ms | FIFO, circular buffer |
| Set insert (100k) | ~5.2 ms | Hash-based deduplication |

Run benchmarks locally:
```bash
cd build && cmake .. -DCOBALT_RUN_TIDY=OFF -DCOBALT_RUN_FORMAT=OFF && make -j8
./tests/bench_runner
```
