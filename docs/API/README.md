# API Reference

Generated API reference documentation for the Cobalt C Framework.

## Structure

| File | Coverage |
|------|----------|
| [cobalt.h.md](cobalt.h.md) | Core Object (L6), Memory/Runtime (L7), Platform (L8) |
| [containers.md](containers.md) | All 10 containers (L4): Vector, List, Stack, Queue, Deque, HashMap, TreeMap, Set, Buffer, LFQueue |
| [interfaces.md](interfaces.md) | Map, Sequence, and Iterator protocols |
| [core.md](core.md) | Object system, class, and interface |
| [memory.md](memory.md) | Allocator and arena |
| [runtime.md](runtime.md) | Error handling and logging |
| [platform.md](platform.md) | Platform detection and atomic ops |
| [algorithm.md](algorithm.md) | Sorting (PDQSort, Heapsort, TimSort-lite), predicates, search, and stream operators (L3) |
| [modules.md](modules.md) | JSON, Event Loop (Timing Wheel & Stream), Work-Stealing Thread Pool, Parallel Sort, and TaskGraph DAG (L1-2) |

## Quick Reference

- **Containers**: See [containers.md](containers.md) for O(1)/O(log n) complexity tables, zero-copy `cobalt_buffer_t`, and lock-free `cobalt_lfqueue_t`
- **Algorithms**: See [algorithm.md](algorithm.md) for PDQSort, Heapsort, TimSort-lite, and `map`/`filter`/`fold` stream operators
- **Objects**: See [core.md](core.md) for ref-count lifecycle and class system
- **Modules**: See [modules.md](modules.md) for JSON parsing, Event Loop (4-level Timing Wheel, async stream), ThreadPool (`parallel_for`, `waitgroup`), Parallel Sort, and TaskGraph DAG Scheduler

- **Strings**: `cobalt_split()`, `cobalt_join()`, `cobalt_strip()` in `utils/string.h`
- **Buffer (Zero-Copy)**: `cobalt_buffer_create()`, `cobalt_buffer_append()`, `cobalt_buffer_prepend()`, `cobalt_buffer_read_fd()`, `cobalt_buffer_write_fd()`
- **Lock-Free Queue**: `cobalt_lfqueue_create()`, `cobalt_lfqueue_enqueue()`, `cobalt_lfqueue_dequeue()`
- **ThreadPool**: `cobalt_threadpool_create()`, `cobalt_threadpool_submit()`, `cobalt_threadpool_parallel_for()`, `cobalt_waitgroup_*()`
- **Parallel Sort**: `cobalt_parallel_qsort()`, `cobalt_parallel_stable_sort()`
- **TaskGraph DAG**: `cobalt_taskgraph_create()`, `cobalt_taskgraph_add_node()`, `cobalt_taskgraph_add_edge()`, `cobalt_taskgraph_compile()`, `cobalt_taskgraph_run_and_wait()`
- **Eventloop & Stream**: `cobalt_eventloop_post()`, `cobalt_stream_create()`, `cobalt_stream_send()`, `cobalt_eventloop_add_timer()`
- **HashMap advanced**: `cobalt_hashmap_set_funcs()`, `cobalt_hashmap_create_ext()`, `*_ext` binary-key variants
- **TreeMap advanced**: `cobalt_treemap_create_ext()` with custom comparator
- **Eventloop IPC**: UNIX domain socket support via `cobalt_eventloop_create_unix_server()` and `cobalt_eventloop_accept()`

## Building

```bash
cd build && cmake .. && cmake --build . --parallel && ctest --output-on-failure
```
