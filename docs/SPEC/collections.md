# SPEC: Concrete Collections (Layer 4)

**Module:** `include/cobalt/container/`, `src/container/`  
**Files:** `vector.h`, `list.h`, `hashmap.h`, `treemap.h`, `stack.h`, `queue.h`, `deque.h`, `set.h`, `buffer.h`, `lfqueue.h`, and corresponding `.c` files

## 1. Overview

The Concrete Collection layer implements robust, high-performance data structures built on top of Layer 5 interface contracts (`Sequence`, `Map`, `Iterator`) and Layer 7 memory allocators.

As of Cobalt v2.5.0, the container family encompasses 10 specialized data structures:
- **Sequential**: Vector (dynamic array), List (doubly-linked), Deque (double-ended list), Stack (LIFO), Queue (FIFO)
- **Associative**: HashMap (chaining hash table), TreeMap (red-black tree), Set (hash set)
- **High-Performance Stream & Concurrency**:
  - `cobalt_buffer_t`: Layer 4 dynamic zero-copy stream buffer with 3-zone sliding layout and vectorized socket I/O
  - `cobalt_lfqueue_t`: Dmitry Vyukov bounded lock-free MPMC queue with cache-line alignment

All collections accept allocator injection at construction time, facilitating whole-heap arena or pool allocation.

---

## 2. Dynamic Zero-Copy Stream Buffer (`cobalt_buffer_t`)

**Header:** `include/cobalt/container/buffer.h`  
**Source:** `src/container/buffer.c`

`cobalt_buffer_t` is a dynamic contiguous stream buffer inspired by Netty `ByteBuf` and Muduo `Buffer`. It solves the impedance mismatch between TCP stream chunking and application-level packet framing.

### 2.1 Three-Zone Architecture

The buffer memory is partitioned into three logical zones delimited by `read_pos` and `write_pos`:

```
+-------------------+------------------+------------------+
|  Prependable Zone |  Readable Zone   |  Writable Zone   |
|   (Header Space)  |    (Payload)     |   (Free Space)   |
+-------------------+------------------+------------------+
0              read_pos           write_pos            capacity
```

- **Prependable Zone** ($[0, \text{read\_pos})$): Space reserved at the head (default 8 bytes) or reclaimed as data is read. Allows prepending protocol framing headers (e.g. 4-byte big-endian packet lengths) without memory reallocations.
- **Readable Zone** ($[\text{read\_pos}, \text{write\_pos})$): Unconsumed payload currently in the buffer. Length is $\text{write\_pos} - \text{read\_pos}$.
- **Writable Zone** ($[\text{write\_pos}, \text{capacity})$): Free space available for appending new bytes or receiving socket reads.

---

### 2.2 In-Place Defragmentation

When appending or reading from a socket, if `writable_bytes < needed`, the buffer checks if the unconsumed readable payload can be shifted left:

```c
size_t prependable = cobalt_buffer_prependable_bytes(buf);
size_t writable = cobalt_buffer_writable_bytes(buf);
if (writable + (prependable - buf->prepend_size) >= len) {
    // In-place compaction: shift readable bytes to prepend_size
    size_t readable = cobalt_buffer_readable_bytes(buf);
    memmove(buf->data + buf->prepend_size, buf->data + buf->read_pos, readable);
    buf->read_pos = buf->prepend_size;
    buf->write_pos = buf->prepend_size + readable;
}
```

This compaction completely avoids costly `realloc()` operations during continuous sliding-window streaming workloads.

---

### 2.3 Two-Segment Vectorized Socket Read (`cobalt_buffer_read_fd`)

To read as much data as possible in a single `readv()` syscall without pre-allocating an excessively large internal buffer:

```c
ssize_t cobalt_buffer_read_fd(cobalt_buffer_t *buf, int fd, int *out_err);
```

1. Prepares a 2-element scatter/gather `struct iovec iov[2]`:
   - `iov[0]`: points to the buffer's writable slice (`data + write_pos`), capacity `writable_bytes`.
   - `iov[1]`: points to a 64KB stack buffer (`char extra_buf[65536]`).
2. Calls `readv(fd, iov, 2)`.
3. If data read fits within `iov[0]`, only `write_pos` is advanced (zero heap allocation, zero copy).
4. If data overflowed into `extra_buf`, `iov[0]` is saturated, and the remaining bytes from `extra_buf` are appended via `cobalt_buffer_append()`, automatically triggering buffer expansion.

---

### 2.4 Vectorized Socket Write (`cobalt_buffer_write_fd`)

```c
ssize_t cobalt_buffer_write_fd(cobalt_buffer_t *buf, int fd, int *out_err);
```

- Directly writes readable bytes from `buf->data + buf->read_pos` to non-blocking `fd`.
- On successful write, automatically advances `read_pos` by the number of bytes written.
- If all bytes are written, resets `read_pos` and `write_pos` to `prepend_size` to maximize future writable space.

---

## 3. Lock-Free Bounded MPMC Queue (`cobalt_lfqueue_t`)

**Header:** `include/cobalt/container/lfqueue.h`  
**Source:** `src/container/lfqueue.c`

`cobalt_lfqueue_t` implements Dmitry Vyukov's bounded Multi-Producer Multi-Consumer (MPMC) lock-free queue using standard C11 atomics (`stdatomic.h`).

### 3.1 Architecture & Cache-Line Alignment

```c
typedef struct {
    atomic_size_t sequence;
    void         *data;
} cobalt_lfcell_t;

struct cobalt_lfqueue {
    _Alignas(64) atomic_size_t head;  // Producer cursor
    _Alignas(64) atomic_size_t tail;  // Consumer cursor
    size_t              mask;
    cobalt_lfcell_t    *buffer;
    cobalt_allocator_t *alloc;
};
```

- **Power-of-Two Ring Buffer**: Capacity is strictly rounded up to a power of two ($2^k$), allowing fast bitwise wrapping: `pos & queue->mask`.
- **False Sharing Elimination**: Producer `head` and consumer `tail` cursors reside on separate 64-byte cache lines (`_Alignas(64)`), eliminating cache bounce contention across CPU cores.
- **Sequence Number Invariant**: Each cell stores an atomic `sequence`. A cell is writable by producer `pos` when `seq == pos`, and readable by consumer `pos` when `seq == pos + 1`.
- **Lock-Free Guarantees**: Non-blocking `cobalt_lfqueue_enqueue` and `cobalt_lfqueue_dequeue` using `atomic_compare_exchange_weak_explicit` with acquire/release memory barriers.

---

## 4. Sequential Collections

### 4.1 Vector (Dynamic Array)
- **Header:** `container/vector.h` | **Source:** `src/container/vector.c`
- **Interfaces:** `Sequence` + `Iterable`
- **Properties**: Contiguous buffer, $O(1)$ random access, amortized $O(1)$ push back (doubles capacity at 100%), $O(N)$ insertion/removal.
- **Memory**: Opaque handle with atomic ref-counting.

### 4.2 List (Doubly-Linked List)
- **Header:** `container/list.h` | **Source:** `src/container/list.c`
- **Interfaces:** `Sequence` + `Iterable`
- **Properties**: Non-contiguous heap nodes, $O(1)$ push/pop at head and tail, $O(1)$ insertion/removal given iterator, $O(N)$ indexed access with head/tail proximity heuristic ($\le N / 2$ hops).

### 4.3 Deque (Double-Ended Queue)
- **Header:** `container/deque.h` | **Source:** `src/container/deque.c`
- **Interfaces:** `Sequence` + `Iterable`
- **Properties**: $O(1)$ push/pop and peek operations at both ends (`push_front`, `push_back`, `pop_front`, `pop_back`).

### 4.4 Stack (LIFO)
- **Header:** `container/stack.h` | **Source:** `src/container/stack.c`
- **Properties**: Singly-linked list implementation, $O(1)$ `push`, `pop`, and `peek`.

### 4.5 Queue (FIFO)
- **Header:** `container/queue.h` | **Source:** `src/container/queue.c`
- **Properties**: Singly-linked list with head and tail pointers, $O(1)$ `enqueue`, `dequeue`, and `peek`.

---

## 5. Associative Collections

### 5.1 HashMap (Hash Table)
- **Header:** `container/hashmap.h` | **Source:** `src/container/hashmap.c`
- **Interfaces:** `Map` + `Iterable`
- **Properties**: Separate chaining hash table, $O(1)$ average get/put/remove, automatic 2x rehash at $75\%$ load factor.
- **Advanced Features**:
  - `cobalt_hashmap_create_ext`: explicit initial bucket count and custom allocator
  - `cobalt_hashmap_set_funcs`: dynamic hash/equality callback replacement
  - Binary key operations (`cobalt_hashmap_put_ext`, `get_ext`, `remove_ext`)

### 5.2 TreeMap (Red-Black Tree)
- **Header:** `container/treemap.h` | **Source:** `src/container/treemap.c`
- **Interfaces:** `Map` + `Iterable`
- **Properties**: Self-balancing red-black tree, guaranteed $O(\log N)$ worst-case get/put/remove, in-order sorted key traversal.
- **Advanced Features**:
  - `cobalt_treemap_create_ext`: injectable custom comparator (`cobalt_compare_func_t`) for non-string keys or case-insensitive sorting.

### 5.3 Set (Hash Set)
- **Header:** `container/set.h` | **Source:** `src/container/set.c`
- **Interfaces:** `Map` + `Iterable`
- **Properties**: Unique element collection backed by hash table, $O(1)$ average insert, remove, and contains checks.

---

## 6. Container Performance & Feature Matrix

| Container | Type | Random Access | Insert/Push Head | Insert/Push Tail | Lookup | Space Complexity | Thread Safety |
|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **Vector** | Dynamic Array | $O(1)$ | $O(N)$ | $O(1)^*$ | $O(N)$ | $O(N)$ contiguous | External sync |
| **List** | Doubly-Linked | $O(N)$ | $O(1)$ | $O(1)$ | $O(N)$ | $O(N)$ nodes | External sync |
| **Deque** | Double-Ended List | $O(N)$ | $O(1)$ | $O(1)$ | $O(N)$ | $O(N)$ nodes | External sync |
| **Stack** | LIFO List | N/A | $O(1)$ | N/A | N/A | $O(N)$ nodes | External sync |
| **Queue** | FIFO List | N/A | N/A | $O(1)$ | N/A | $O(N)$ nodes | External sync |
| **HashMap** | Chaining Hash Table | N/A | N/A | N/A | $O(1)$ avg | $O(N)$ buckets | External sync |
| **TreeMap** | Red-Black Tree | N/A | N/A | N/A | $O(\log N)$ | $O(N)$ nodes | External sync |
| **Set** | Hash Set | N/A | N/A | N/A | $O(1)$ avg | $O(N)$ buckets | External sync |
| **Buffer** | Dynamic Stream Buffer | $O(1)$ peek | $O(1)$ prepend | $O(1)$ append | N/A | $O(N)$ contiguous | External sync |
| **LFQueue** | Bounded Lock-Free MPMC | N/A | N/A | $O(1)$ lock-free | N/A | $O(\text{cap})$ cache-aligned | **Lock-Free MPMC** |

$^*$ Amortized $O(1)$
