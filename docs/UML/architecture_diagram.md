# UML: Cobalt Architecture Baseline Diagrams (v2.0.0)

## 1. Layer Dependency Graph (Directed Acyclic Graph)

```
+---------------------------------------------------+
|              L1: Applications & Extensions        |
|  (uses: L2,L3,L4,L5,L6,L7,L8)                     |
+-----------------------+---------------------------+
                        ▲
                        │ (depends on)
+-----------------------+---------------------------+
|              L2: Modules & Utilities              |
|  (uses: L3,L4,L5,L6,L7,L8)                       |
+-----------------------+---------------------------+
                        ▲
                        │
+-----------------------+---------------------------+
|              L3: Algorithms & Streams             |
|  (uses: L4(L5),L6,L7,L8)                         |
+-----------------------+---------------------------+
                        ▲
                        │
+-----------------------+---------------------------+
|           L4: Concrete Collections                |
|  (implements L5, uses L6,L7,L8)                   |
+-----------------------+---------------------------+
                        ▲ (implements/enforces)
                        │
+-----------------------+---------------------------+
|            L5: Container Interfaces               |
|  (pure abstract, used by L3,L4)                   |
+-----------------------+---------------------------+
                        ▲ (extends/inherits)       │ (polymorphic dispatch)
                 +-------+-------+                  │
                 ▼               ▼                  │
+----------------+-----------+ +------------------+----+
|          L6a: Object System  |    L7a: Memory     |
| (class hier, RTTI, refs)     | (allocators, arenas)|
+----------------+-----------+ +------------------+----+
                 ▲                           ▲
                 │ (composed)                │ (used by)
         +-------+-------+           +--------+---------+
         │ L7b: Runtime  │           │ L8: Platform     |
         | (errors, log) |           | (OS/atomic/align)|
         +---------------+           +------------------+
```

Arrow direction: upper layer depends on lower layer. No upward or lateral arrows permitted.

## 2. Class Hierarchy UML (Simplified Text Representation)

```
+------------------------------------------+
|            cobalt_object_t               |
|------------------------------------------|
| - ref_count: uint64_t                    |
| - class: cobalt_class_t*                 |
|------------------------------------------|
| + cobalt_object_new(cls, extra_size)     |
| + cobalt_object_ref(obj)                 |
| + cobalt_object_unref(obj)               |
| + cobalt_object_get_class(obj)           |
+------------------------------------------+
                       ^
                       | (single inheritance)
                       |
   +-------------------------------------+
   |          cobalt_class_t             |
   |-------------------------------------|
   | - name: const char*                 |
   | - method_count: size_t              |
   | - methods: cobalt_method_t**        |
   | - property_count: size_t            |
   | - properties: cobalt_property_t**   |
   | - base_class: cobalt_class_t*       |
   | - abstract: int                     |
   |-------------------------------------|
   | + cobalt_class_create(name, base)   |
   | + cobalt_class_add_method()         |
   | + cobalt_class_destroy()            |
   +-------------------------------------+
                       ^
                       | (multiple inheritance via interfaces)
                       |
   +-------------------------------------+
   |    cobalt_interface_t (abstract)    |
   |-------------------------------------|
   | - vtable: cobalt_interface_vtable_t*|
   |-------------------------------------|
   | + cobalt_interface_destroy()        |
   +-------------------------------------+

Example concrete class hierarchy:

cobalt_object_t (base)
   ↑
CobaltSequence (base for all sequence-like objects)
   ↑
CobaltVector  ---- implements --> CobaltMap interface
CobaltList    ---- implements --> CobaltMap interface
```

## 3. Object Lifecycle Sequence Diagram

```
Application          CobaltObject       ClassMetadata       Allocator
     |                   |                  |                   |
     |---- create ----->|                  |                   |
     |                   |---- alloc ----->|                   |
     |                   |                  |---- alloc ------> |
     |                   |                  |                   |---- return ---->
     |                   |---- init ------>|                   |
     |                   |---- set class -->|                   |
     |<--- object ptr --|                  |                   |
     |                   |                                                  |
     |---- use -------->|                                                  |
     |                   |                                                  |
     |---- unref ------>|                  |                               |
     |                   |---- dec_ref ---->|                             |
     |                   |                  |---- free? --------------> |
     |                   |                                                  |
     |<-- cleanup done --|                  |                   |
```

Steps:
1. Application requests object creation via `cobalt_object_new(class, extra_size)`
2. Object allocates memory (size = sizeof(cobalt_object_t) + extra) using injected allocator
3. Object header is initialized: ref_count = 1, class pointer set
4. Extra payload space is left uninitialized (application initializes)
5. Application obtains pointer and uses the object
6. When done, application calls `cobalt_object_unref()` — decrementing ref_count
7. When ref_count reaches 0, memory is returned to the allocator

## 4. HashMap Build Sequence (Allocator Injection)

```
Application             ArenaAlloc         HashMap
     |                       |                  |
     |--- create arena ----->|                  |
     |                       |---- alloc -----> |
     |                       |                  |---- buf ---------->
     |<--- arena handle ------|                  |
     |                       |                  |
     |--- create_hashmap ---->|                  |
     |                       |---- alloc -----> | (bucket array)
     |                       |                  |---- buckets ----->
     |<--- hashmap ---------  |                  |
     |                       |                  |
     |--- put(key,value) ---->|                  |
     |                       |---- get_bucket |
     |                       |---- insert ---->|
     |<------ true ----------  |                  |
```

Key point: Every allocation in hashmap construction comes from the arena allocator — no heap (`malloc`) is invoked directly.

## 5. Event Loop FD Registration Diagram

```
Application           EventLoop         Platform(OS)
     |                     |                  |
     |--- create_loop ---->|                  |
     |--- epoll/kqueue ---|---- open_fd ---->|
     |<--- epfd handle ----|                  |
     |                     |                  |
     |--- add_fd(epfd,fd,events,cb,ctx) ---->|
     |                     |---- register -->| (epoll_ctl/KQUEUE)
     |                     |                  |
     |                     |---- run loop --->| (select/epoll_wait)
     |                     |                  |
     | <--- fd ready cb --| (callbacks run here)
     |                     |                  |
     |--- stop_loop ------>|                  |
     |--- close_epoll ---->| ---- close_fd -->|
```

The event loop abstracts platform-specific multiplexing APIs (epoll on Linux, kqueue on BSD/macOS, IOCP on Windows) behind a uniform interface.