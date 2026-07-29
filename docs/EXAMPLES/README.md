# Cobalt Framework Examples

This directory contains working example programs demonstrating various aspects of the Cobalt C framework.

## Usage

To compile an example:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make example_name
./../docs/EXAMPLES/examples/<subdir>/<example_name>
```

Or directly with the source file:

```bash
gcc -Iinclude -Lbuild/src -lcobalt docs/EXAMPLES/examples/basic/hello_world.c -o hello_world
./hello_world
```

## Example Categories

### basic/
- `hello_world.c` - Simple "Hello World" showing platform detection and logging

### oop/
- `class_hierarchy.c` - Object-oriented programming pattern showing class inheritance and reference counting

### containers/
- `vector_demo.c` - Using the Vector (dynamic array) container
- `hashmap_demo.c` - Using the HashMap (key-value store) container

### algorithms/
- `sort_example.c` - Using sorting algorithms (qsort, insertion_sort) from the algorithm layer

### json/
- `json_example.c` - JSON parsing and serialization using the JSON module

### eventloop/
- `simple_server.c` - Basic event loop usage with timer callbacks

### memory/
- `arena_allocator.c` - Demonstrating arena-based memory allocation for frame-scoped workloads

### threads/
- `refcount_demo.c` - Thread-safe reference counting demonstration using atomic operations

### app_template/
- `application_skeleton.c` - Production-style application template showing initialization, main loop, and graceful shutdown
