# SPEC: Concrete Collections (Layer 4)

**Module:** `include/cobalt/container/`, `src/container/`  
**Files:** vector.h, list.h, hashmap.h, treemap.h, and corresponding .c files

## 1. Overview

Concrete collection types implement the Layer 5 interface contracts. Each is optimized for different access patterns and use cases.

## 2. Vector (Dynamic Array)

**Header:** `container/vector.h`  
**Source:** `src/container/vector.c`  
**Interfaces:** `Sequence` + `Iterable`

Properties:
- Contiguous memory storage (cache-friendly)
- O(1) random access by index
- Amortized O(1) append (push_back)
- O(n) insertion/removal in middle
- Grows capacity by doubling when full

Typical use: when you need fast indexed access and mostly append-only growth.

## 3. List (Doubly-Linked List)

**Header:** `container/list.h`  
**Source:** `src/container/list.c`  
**Interfaces:** `Sequence` + `Iterable`

Properties:
- Non-contiguous node allocation
- O(1) front/back push/pop
- O(n) random access (linear scan)
- Constant-time insert/remove given iterator/node position
- Higher memory overhead per element (prev+next pointers)

Typical use: frequent insert/delete at head/tail, or when you need stable node references.

## 4. HashMap (Hash Table)

**Header:** `container/hashmap.h`  
**Source:** `src/container/hashmap.c`  
**Interfaces:** `Map` + `Iterable`

Properties:
- Key-based lookup (string keys, hashed to bucket)
- O(1) average-case get/put/remove
- No guaranteed order iteration
- Collision chaining handles hash conflicts
- Load factor threshold triggers rehashing

Typical use: fast key-value lookup where ordering doesn't matter.

## 5. TreeMap (Red-Black Tree)

**Header:** `container/treemap.h`  
**Source:** `src/container/treemap.c`  
**Interfaces:** `Map` + `Iterable`

Properties:
- Self-balancing binary search tree (red-black)
- O(log n) guaranteed get/put/remove
- Keys maintained in sorted order
- Iterator traverses in-key-order

Typical use: when you need ordered key traversal with guaranteed logarithmic performance.

## 6. Allocator Integration

All collections accept allocator injection at construction. For example:

```c
// Create a vector using arena allocator
cobalt_allocator_t *arena_alloc = get_arena_allocator();
cobalt_vector_t *vec = vector_create_with_allocator(arena_alloc, 16);
```

All internal allocations go through the provided allocator, enabling whole-heap-free operation when combined with an arena/pool allocator.

## 7. Future Collections (Planned)

- Stack (LIFO queue) — based on vector or list
- Queue (FIFO) — based on circular buffer or doubly-linked list
- Set (unique-key collection) — could wrap HashMap or use separate data structure
