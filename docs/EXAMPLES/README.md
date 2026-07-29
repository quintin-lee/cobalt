# Cobalt Framework Usage Examples

This directory contains example programs demonstrating how to use various components of the Cobalt C framework.

## Getting Started

To compile and run examples:

```bash
cd cobalt/examples
cmake .. && make
./example_name
```

See individual subdirectory README.md files for specific instructions.

## Example Categories

### 1. Hello World / Basic Setup

**Location:** `basic/hello_world.c`

Demonstrates:
- Simple inclusion of `<cobalt/cobalt.h>`
- Platform detection via `cobalt_platform_get_id()`
- Basic allocator usage (`cobalt_allocator_get_system()`)
- Simple logging with `cobalt_logger_init()` and macros

### 2. Object-Oriented Programming Patterns

**Location:** `oop/class_hierarchy.c`

Demonstrates:
- Creating class hierarchies with `cobalt_class_create()`
- Adding methods to classes with `cobalt_class_add_method()`
- Creating objects with `cobalt_object_new()`
- Reference counting lifecycle (`cobalt_object_ref`/`unref`)
- Interface querying with `cobalt_object_query_interface()`

### 3. Container Usage Patterns

**Location:** `containers/vector_demo.c`, `containers/hashmap_demo.c`

Demonstrates:
- Creating vectors/lists/hashmaps/trees with appropriate constructors
- Performing basic operations (push, get, set, iterate)
- Using iterators to traverse collections
- Allocator injection for arena-based container lifecycles

### 4. Algorithm Application

**Location:** `algorithms/sort_example.c`

Demonstrates:
- Defining comparison functions for `qsort`/`insertion_sort`
- Sorting arrays of custom structures
- Using predicate functions for filtering/searching

### 5. JSON Serialization

**Location:** `json/json_example.c`

Demonstrates:
- Parsing JSON text into in-memory tree via `json_parse()`
- Navigating and extracting values from the JSON tree
- Serializing back to string via `json_serialize()`
- Properly destroying allocated nodes with `json_destroy()`

### 6. Event Loop Integration

**location:** `eventloop/simple_server.c`

Demonstrates:
- Creating an event loop with `cobalt_eventloop_create()`
- Registering file descriptor handlers
- Setting up timers
- Running the loop and handling events
- Clean shutdown with `cobalt_eventloop_stop()` and `destroy()`

### 7. Memory Autonomy with Custom Allocators

**Location:** `memory/arena_allocator.c`

Demonstrates:
- Creating an arena with fixed-size buffer
- Allocating multiple objects from the arena in sequence
- Resetting the arena to free everything at once
- Benefits for frame-by-frame workloads (games, real-time systems)

### 8. Thread-Safe Reference Counting Demo

**Location:** `threads/refcount_demo.c`

Demonstrates:
- Multiple threads incrementing/decrementing shared object refcounts
- Using atomic operations safely without additional locks
- Demonstrating that reference counting is lock-free on the platform

### 9. Production-Style Application Skeleton

**Location:** `app_template/application_skeleton.c`

Demonstrates a typical application structure using Cobalt:
- Early initialization (platform, allocator, logger, core classes)
- Service/component construction layered over the framework
- Main event-driven or polling loop
- Graceful shutdown with resource cleanup

## Contributing Examples

New examples should:
1. Be self-contained and compile with `make`
2. Demonstrate one clear concept or pattern
3. Include comments explaining key lines
4. Follow the project's coding standards
5. Have a short README.md in their subdirectory describing what they show

Submit via pull request following the contribution process described in the Developer Guide.