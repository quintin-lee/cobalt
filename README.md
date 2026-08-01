# Cobalt C Framework

A lightweight, zero-dependency C11 framework providing object-oriented capabilities, memory management, and cross-platform abstractions.

## Features

- **8-Layer Architecture**: Clean separation of concerns
- **Object System**: Single inheritance + multi-interface support
- **Memory Management**: Arena allocator with automatic cleanup
- **Thread Safety**: Atomic operations with stdatomic.h
- **Containers**: Vector, List, HashMap, TreeMap, Stack, Queue
- **Algorithms**: Sort, predicates, functional utilities
- **Modules**: JSON parser/serializer, event loop (epoll/kqueue)
- **Cross-platform**: Linux, macOS, Windows support

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
make test
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

## Testing

```bash
cd build
ctest --output-on-failure
```

## Documentation

- [RFC](docs/RFC/) - Architecture decisions
- [SPEC](docs/SPEC/) - Module specifications
- [API](docs/API/) - Public API reference
- [Examples](docs/EXAMPLES/) - Usage examples
- [Tutorial](docs/TUTORIAL/) - Getting started guide

## License

MIT License - See [LICENSE](LICENSE) for details.
