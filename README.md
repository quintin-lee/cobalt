# Cobalt C Framework

A lightweight, zero-dependency C11 framework providing object-oriented capabilities, memory management, and cross-platform abstractions.

## Features

- **8-Layer Architecture**: Strict upward-only dependency model
- **Object System**: Single inheritance + multi-interface support with reference counting
- **Memory Management**: System, Arena, Pool, and Slab allocators with custom allocator injection, and 3-Epoch Safe Memory Reclamation (`cobalt_ebr_t`)
- **Thread Safety**: Atomic operations with `stdatomic.h` and platform primitives
- **Shared Memory IPC Channel**: Zero-copy lock-free cross-process communication (`cobalt_shm_channel_t`) with dual topologies (variable-length SPSC ring buffer & Dmitry Vyukov MPMC slot queue), configurable runtime wait policies (SPIN, BLOCKING, ADAPTIVE), kernel syscall coalescing, and seamless EventLoop reactor integration
- **Containers**: Vector, List, HashMap, TreeMap, Stack, Queue, Set, Deque, LFQueue (lock-free MPMC queue), Buffer (zero-copy dynamic stream buffer), Lock-Free SkipList (`cobalt_lfskiplist_t`), and Lock-Free SHM Rings (`cobalt_shm_spsc_t`, `cobalt_shm_mpmc_t`)
- **Algorithms**: Pattern-Defeating QuickSort (PDQSort), Introspective Heapsort, Adaptive Run MergeSort (TimSort-lite), binary search, find_if, for_each, predicates
- **Parallel Algorithms**: Multi-core in-place parallel PDQSort (`cobalt_parallel_qsort`) and 100% stable parallel MergeSort (`cobalt_parallel_stable_sort`) with adaptive cutoff heuristics and Help-First work-stealing barriers
- **TaskGraph DAG Scheduler**: Event-driven Directed Acyclic Graph task scheduler (`cobalt_taskgraph_t`) with Kahn's algorithm cycle detection, zero-reallocation graph reuse, and atomic in-degree push dispatch
- **Thread Pool**: Work-stealing task scheduler with Chase-Lev lock-free deques, waitgroups, and dynamic parallel-for loops
- **Event Loop & Asynchronous I/O**: High-performance reactor with 4-level hierarchical timing wheel ($O(1)$ add/cancel), `eventfd` async task dispatch, buffered stream abstraction (`cobalt_stream_t`), and shared memory channel integration
- **Modules**: JSON DOM & SAX parser/serializer, Work-Stealing Thread Pool, Event loop (epoll/kqueue), Parallel Sort, DAG TaskGraph Scheduler, Shared Memory IPC Channel
- **Cross-platform**: Linux (epoll, eventfd, shm), macOS (kqueue), Windows support
- **String utilities**: Split, join, and strip with `cobalt_split()`, `cobalt_join()`, `cobalt_strip()`
- **HashMap runtime replacement**: Swap hash/equal functions at runtime via `cobalt_hashmap_set_funcs()`
- **TreeMap custom comparator**: Generic key comparison with `cobalt_treemap_create_ext()`
- **UNIX domain sockets**: Local IPC with `cobalt_eventloop_create_unix_server()` and `cobalt_eventloop_accept()`

## Architecture

```
L8  Platform       → Platform detection, atomic operations, thread primitives, SHM (cobalt_shm_t), Notifier (cobalt_notifier_t)
L7a Memory         → System allocator, Arena allocator, Pool, Slab, EBR engine
L7b Runtime        → Error handling, Structured logging, Thread-local state
L6  Core           → Object system, Class system, Interface tables
L5  Interface      → Sequence, Map, Set, Iterator protocols
L4  Container      → Vector, List, HashMap, TreeMap, Stack, Queue, Set, Deque, Buffer, LFQueue, SkipList, SHM Ring (SPSC/MPMC)
L3  Algorithm      → PDQSort, Heapsort, TimSort, Search, Predicates, Functional stream operators, Pike VM Regex
L2  Module         → JSON (DOM/SAX), Thread Pool, Event Loop, Parallel Sort, TaskGraph DAG, SHM Channel (cobalt_shm_channel_t)
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
# Disable the warning-as-error gate for a permissive local build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCOBALT_STRICT_WARNINGS=OFF
```

```bash
# Disable the clang-tidy / clang-format build gates for a specific local build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCOBALT_RUN_TIDY=OFF -DCOBALT_RUN_FORMAT=OFF
```

> Note: `-Wall -Wextra -Werror` is enabled by default via the `COBALT_STRICT_WARNINGS`
> option (default `ON`). The optional `COBALT_RUN_TIDY` / `COBALT_RUN_FORMAT` flags
> (default `ON`) control whether clang-tidy and the clang-format POST_BUILD check run.

## Quick Start

```c
#include <cobalt/cobalt.h>

int main(void) {
    // Platform detection
    cobalt_platform_id_t platform = cobalt_platform_get_id();
    (void)platform; // silence unused-variable warning

    // Create a vector of ints with initial capacity 5
    cobalt_vector_t *vec = cobalt_vector_create(5);
    int value = 42;
    cobalt_vector_push(vec, &value);

    // Create a hashmap with 16 buckets
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
ctest --output-on-failure  # 45 unit suites + 18 benchmarks passing (100%)
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
| Source files (`src/**/*.c`) | 48 |
| Header files (`include/**/*.h`) | 50 |
| Test files (`tests/**/*.c`) | 67 |
| Benchmark files (`tests/benchmark/*.c`) | 20 |
| Example files (`examples/*.c`) | 17 |
| Documentation files (`docs/**/*.md`, `*.md`) | 75 |

| Metric | Value |
|--------|-------|
| Total lines of code (src + include) | ~23,000 |
| Git commits | 470+ |
| CTest status | 63/63 passing (100%: 45 unit suites + 18 benchmarks) |
| Sanitizer status | Clean (ASan/LSan 0 leaks, 0 errors) |


## CI/CD

Continuous integration runs on every push to `master`:

| Job | Description |
|-----|-------------|
| **build** | Build with GCC/Clang × Debug/Release (4 variants) with format + tidy gates |
| **asan** | AddressSanitizer build with LSan suppressions for allocator-inject tests |
| **ubsan** | UndefinedBehaviorSanitizer build |
| **tsan** | ThreadSanitizer build for data race detection |
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
| Conan | `conan install --requires=cobalt/2.7.0` |
| pkg-config | `pkg-config --cflags --libs cobalt` |

## Performance

Benchmark results from a single reference machine (x86_64 Linux, Release build, averaged across iterations):

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
| **SHM SPSC Ping-Pong (Spin)** | 100k rounds | ~214 ns RTT | Lock-free user-space spin, 0 syscalls |
| **SHM SPSC Ping-Pong (Blocking)** | 10k rounds | ~15.5 µs RTT | Eventfd/FIFO sleeping with syscall coalescing |
| **SHM Throughput (64B)** | 1M messages | ~22.1M msgs/sec (1.32 GB/s) | Lock-free SPSC zero-copy streaming |
| **SHM Throughput (1KB)** | 200k messages | ~18.7M msgs/sec (17.88 GB/s) | Medium payload streaming |
| **SHM Throughput (64KB)** | 20k messages | ~10.1M msgs/sec (618.5 GB/s) | Large zero-copy frame throughput |

> **Note**: These are indicative measurements from a single x86_64 Linux reference machine (Release build, averaged across iterations). Actual throughput and latency vary with CPU model, core count, compiler version, and system load — treat them as approximate guidance rather than guarantees, and reproduce locally with the commands below.

Run benchmarks locally:
```bash
cd build && cmake .. -DCOBALT_RUN_TIDY=OFF -DCOBALT_RUN_FORMAT=OFF && make -j8
./tests/bench_runner
./tests/benchmark_sort
./tests/benchmark_parallel_sort
./tests/benchmark_taskgraph
./tests/benchmark_threadpool
./tests/benchmark_eventloop
./tests/benchmark_shm
```

## Shared Memory IPC Channel

Cobalt provides a high-throughput, sub-microsecond shared memory IPC channel (`cobalt_shm_channel_t`) with zero dynamic heap allocations on the data transfer path.

### Key Capabilities
- **Dual Topologies**: Single-Producer Single-Consumer (`COBALT_SHM_MODE_SPSC`) variable-length ring buffer with zero-copy reserve/commit/peek/consume, and Multi-Producer Multi-Consumer (`COBALT_SHM_MODE_MPMC`) Dmitry Vyukov sequence-slot queue.
- **Configurable Wait Policies**: `COBALT_SHM_WAIT_SPIN` (0 syscalls, sub-microsecond latency), `COBALT_SHM_WAIT_BLOCKING` (eventfd/FIFO sleep with automatic reader/writer tracking), and `COBALT_SHM_WAIT_ADAPTIVE` (configurable spin threshold before OS descriptor sleep).
- **Kernel Syscall Coalescing**: When receivers are actively spinning or draining queues, publishers skip OS notification syscalls entirely, maximizing pipeline throughput.
- **Reactor Integration**: Easily attach shared memory channels to the Cobalt `cobalt_eventloop_t` reactor for non-blocking asynchronous event dispatch.

### Quick Example (Zero-Copy SPSC Stream)

```c
#include <cobalt/cobalt.h>

// Server/Creator Process
cobalt_shm_config_t cfg = {
    .name              = "/my_ipc_bus",
    .mode              = COBALT_SHM_MODE_SPSC,
    .wait_policy       = COBALT_SHM_WAIT_ADAPTIVE,
    .capacity          = 65536,
    .create_if_missing = true,
};
cobalt_shm_channel_t *server = cobalt_shm_channel_create(&cfg);

// Zero-copy reserve directly in shared memory
void *payload = NULL;
cobalt_shm_channel_reserve(server, sizeof(my_struct_t), &payload);
((my_struct_t *)payload)->id = 1001;
cobalt_shm_channel_commit(server, 1 /* msg_type */, sizeof(my_struct_t));

// Client/Consumer Process
cobalt_shm_channel_t *client = cobalt_shm_channel_open("/my_ipc_bus", COBALT_SHM_WAIT_ADAPTIVE);
uint16_t msg_type = 0;
const void *read_ptr = NULL;
size_t len = 0;
if (cobalt_shm_channel_peek(client, &msg_type, &read_ptr, &len) == COBALT_SUCCESS) {
    const my_struct_t *data = (const my_struct_t *)read_ptr;
    printf("Received ID: %d\n", data->id);
    cobalt_shm_channel_consume(client);
}

// Cleanup
cobalt_shm_channel_destroy(client);
cobalt_shm_channel_destroy(server);
```
