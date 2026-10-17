# Cobalt C Framework

A lightweight, zero-dependency C11 framework providing object-oriented capabilities, memory management, and cross-platform abstractions.

## Features

- **8-Layer Architecture**: Strict upward-only dependency model
- **Object System**: Single inheritance + multi-interface support with reference counting
- **Memory Management**: System, Arena, Pool, and Slab allocators with custom allocator injection
- **Thread Safety**: Atomic operations with `stdatomic.h` and platform primitives
- **Containers**: Vector, List, HashMap, TreeMap, Stack, Queue, Set, Deque, LFQueue (lock-free MPMC queue), and Buffer (zero-copy dynamic stream buffer)
- **Algorithms**: Pattern-Defeating QuickSort (PDQSort), Introspective Heapsort, Adaptive Run MergeSort (TimSort-lite), binary search, find_if, for_each, predicates
- **Parallel Algorithms**: Multi-core in-place parallel PDQSort (`cobalt_parallel_qsort`) and 100% stable parallel MergeSort (`cobalt_parallel_stable_sort`) with adaptive cutoff heuristics and Help-First work-stealing barriers
- **TaskGraph DAG Scheduler**: Event-driven Directed Acyclic Graph task scheduler (`cobalt_taskgraph_t`) with Kahn's algorithm cycle detection, zero-reallocation graph reuse, and atomic in-degree push dispatch
- **Thread Pool**: Work-stealing task scheduler with Chase-Lev lock-free deques, waitgroups, and dynamic parallel-for loops
- **Event Loop & Asynchronous I/O**: High-performance reactor with 4-level hierarchical timing wheel ($O(1)$ add/cancel), `eventfd` async task dispatch, and buffered stream abstraction (`cobalt_stream_t`)
- **Modules**: JSON DOM & SAX parser/serializer, Work-Stealing Thread Pool, Event loop (epoll/kqueue), Parallel Sort, DAG TaskGraph Scheduler
- **Cross-platform**: Linux (epoll, eventfd), macOS (kqueue), Windows support
- **String utilities**: Split, join, and strip with `cobalt_split()`, `cobalt_join()`, `cobalt_strip()`
- **HashMap runtime replacement**: Swap hash/equal functions at runtime via `cobalt_hashmap_set_funcs()`
- **TreeMap custom comparator**: Generic key comparison with `cobalt_treemap_create_ext()`
- **UNIX domain sockets**: Local IPC with `cobalt_eventloop_create_unix_server()` and `cobalt_eventloop_accept()`

## Architecture

```
L8  Platform       → Platform detection, atomic operations, thread primitives
L7a Memory         → System allocator, Arena allocator, Pool, Slab
L7b Runtime        → Error handling, Structured logging, Thread-local state
L6  Core           → Object system, Class system, Interface tables
L5  Interface      → Sequence, Map, Set, Iterator protocols
L4  Container      → Vector, List, HashMap, TreeMap, Stack, Queue, Set, Deque, Buffer, LFQueue
L3  Algorithm      → PDQSort, Heapsort, TimSort, Search, Predicates, Functional stream operators
L2  Module         → JSON (DOM/SAX), Thread Pool, Event Loop, Parallel Sort, TaskGraph DAG
L1  Application    → Example applications, Benchmarks
```

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

### Strict warnings

The default CMake configuration enables `-Wall -Wextra -Werror` to keep the codebase clean and portable.

```bash
# Disable strict warnings if needed for a specific local build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCOBALT_STRICT_WARNINGS=OFF
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
ctest --output-on-failure  # 54/54 tests passing (100%)
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
| Source files (`src/**/*.c`) | 40 |
| Header files (`include/**/*.h`) | 42 |
| Test files (`tests/**/*.c`) | 58 |
| Benchmark files (`tests/benchmark/*.c`) | 16 |
| Example files (`examples/*.c`) | 17 |
| Documentation files (`docs/**/*.md`, `*.md`) | 43 |

| Metric | Value |
|--------|-------|
| Total lines of code (src + include) | ~20,100 |
| Git commits | 450+ |
| CTest status | 54/54 passing (100%) |
| Sanitizer status | Clean (ASan/LSan 0 leaks, 0 errors) |


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
| Conan | `conan install --requires=cobalt/2.5.0` |
| pkg-config | `pkg-config --cflags --libs cobalt` |

## Performance

Benchmark results (x86_64 Linux, Release build, averaged across iterations):

| Component / Operation | Workload / Size | Throughput / Time | Notes |
|-----------------------|-----------------|-------------------|-------|
| **Vector push** | 100k items | ~1.5 ms | Contiguous memory, cache-friendly |
| **Vector get** | 100k items | ~2.0 ms | O(1) direct indexing |
| **HashMap put** | 100k string keys | ~92 ms | Separate chaining, bucket masking |
| **HashMap get** | 100k string keys | ~100 ms | Average O(1) hash table lookup |
| **TreeMap put** | 100k string keys | ~173 ms | Red-Black self-balancing BST |
| **TreeMap get** | 100k string keys | ~212 ms | Guaranteed O(log n) lookup |
| **List push_back** | 100k items | ~10.8 ms | Doubly-linked node insertion |
| **Deque push_front** | 100k items | ~8.9 ms | Constant time front insertion |
| **Stack push** | 100k items | ~11.9 ms | LIFO singly-linked push |
| **Queue enqueue** | 100k items | ~7.9 ms | FIFO circular buffer / list |
| **Set insert** | 100k items | ~4.9 ms | Hash-based deduplication |
| **LFQueue enqueue/dequeue** | 100k items | ~5.0 ms | Lock-free bounded MPMC queue |
| **PDQSort (Random)** | 100k integers | ~69 ms | Introspective Pattern-Defeating QuickSort |
| **PDQSort (Pre-sorted)** | 100k integers | ~46 ms | O(N) linear time on presorted data |
| **Adaptive MergeSort** | 100k integers | ~43 ms | TimSort-lite 100% stable sorting |
| **Parallel PDQSort** | 1M integers (8 threads) | ~294 ms (3.40M elem/s) | 3.37x multi-core speedup |
| **Parallel Stable Sort** | 1M integers (4 threads) | ~270 ms (3.70M elem/s) | 2.84x multi-core speedup, 100% stable |
| **TaskGraph DAG (Fan-out/in)**| 10k tasks | ~1.96M tasks/sec | Pure event-driven atomic in-degree dispatch |
| **TaskGraph DAG (Pipeline)** | 10k tasks (500x20) | ~4.64M tasks/sec | Multi-layer pipelined DAG execution |
| **ThreadPool Task Dispatch** | 100k tasks (4 threads) | ~1.82M tasks/sec | Hybrid Chase-Lev work-stealing |
| **ThreadPool Parallel-For** | 100k items | ~1.2 ms | Dynamic chunked parallel loop |
| **Timing Wheel Add** | 100k timers | ~1.21M ops/sec | 4-level bitwise wheel (O(1) insert) |
| **Timing Wheel Cancel** | 50k timers | ~2.47M ops/sec | Thomas Wang hash table (O(1) delete) |
| **Cross-Thread Post** | 100k tasks | ~1.99M tasks/sec | Lock-free queue + eventfd wakeup |
| **Zero-Copy Buffer Stream**| 488 MB streamed | ~9.26 GB/sec | 3-zone ring buffer, zero realloc |

Run benchmarks locally:
```bash
cd build && cmake .. -DCOBALT_RUN_TIDY=OFF -DCOBALT_RUN_FORMAT=OFF && make -j8
./tests/bench_runner
./tests/benchmark_sort
./tests/benchmark_parallel_sort
./tests/benchmark_taskgraph
./tests/benchmark_threadpool
./tests/benchmark_eventloop
```
