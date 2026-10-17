# Design Specification: Lock-Free Zero-Copy Shared Memory IPC Channel (`cobalt_shm_channel_t`)

- **Author**: Antigravity & Cobalt Team
- **Date**: 2026-10-17
- **Target Release**: Cobalt v2.8.0
- **Status**: Ready for Implementation
- **Category**: Layer 8 (Platform) + Layer 4 (Container) + Layer 2 (Module)

---

## 1. Executive Summary & Goals

Inter-Process Communication (IPC) in traditional high-performance systems often relies on Unix Domain Sockets (UDS), pipes, or message queues. These primitives incur significant overhead due to kernel mode switches, buffer copies, and context switching (typical round-trip latency 2,000~6,000 ns).

This specification defines a pure C11, cross-platform, zero-copy, lock-free shared memory (shm) IPC channel (`cobalt_shm_channel_t`) for Cobalt v2.8.0.

### Key Objectives:
1. **Ultra-Low Latency**: Sub-80ns round-trip latency in pure SPSC spin mode, 20M+ msgs/sec throughput.
2. **Dual-Mode Topology**:
   - **SPSC Core**: Single-Producer Single-Consumer lock-free ring buffer with zero CAS operations, zero intermediate memory copies, and variable-length message framing.
   - **MPMC Extension**: Multi-Producer Multi-Consumer Dmitry Vyukov sequence-slot ring buffer for multi-process event buses.
3. **Strict Layered Architecture (L8 -> L4 -> L2)**:
   - **Layer 8 (Platform)**: Cross-platform OS shared memory (`cobalt_shm_t`) and cross-process notification primitive (`cobalt_notifier_t`).
   - **Layer 4 (Container)**: Lock-free SPSC and MPMC shared memory ring data structures (`cobalt_shm_ring_t`).
   - **Layer 2 (Module)**: High-level channel interface (`cobalt_shm_channel_t`) with configurable wait policies and seamless `cobalt_eventloop_t` integration.
4. **Three Wait/Wakeup Policies**:
   - `COBALT_SHM_WAIT_SPIN`: 100% busy-wait for ultra-low latency (HFT / dedicated core).
   - `COBALT_SHM_WAIT_BLOCKING`: OS descriptor sleep for CPU/battery conservation.
   - `COBALT_SHM_WAIT_ADAPTIVE`: Microsecond-scale spin-yield fallback to OS signaling.
5. **Syscall Coalescing**:
   - Senders check `waiting_readers` atomic counter; OS notifications are skipped if the receiver is actively consuming.

---

## 2. Architecture & Layering

```
+-------------------------------------------------------------------------+
| Layer 2 (Module): cobalt_shm_channel_t                                  |
|   - Channel lifecycle (create/open/destroy)                             |
|   - Wait policy state machine (SPIN / BLOCKING / ADAPTIVE)             |
|   - EventLoop bridge (cobalt_shm_channel_attach_eventloop)             |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Layer 4 (Container): cobalt_shm_spsc_t & cobalt_shm_mpmc_t              |
|   - SPSC: Variable-length zero-copy reserve/commit & wrap-around marker |
|   - MPMC: Dmitry Vyukov atomic sequence-slot bounded ring buffer       |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Layer 8 (Platform): cobalt_shm_t & cobalt_notifier_t                    |
|   - shm: POSIX shm_open/mmap (Linux/macOS) & CreateFileMapping (Win)    |
|   - notifier: eventfd (Linux), socketpair/pipe (macOS), Event (Windows) |
+-------------------------------------------------------------------------+
```

---

## 3. Shared Memory Physical Layout & Header Specification

The shared memory region is mapped to a contiguous virtual address space, aligned to 64-byte cache lines.

```
+---------------------------------------------------------------------------------------+
|                                  Shared Memory Physical Layout                        |
+---------------------------------------------------------------------------------------+
|  [Cache Line 0-1]   cobalt_shm_header_t: Magic, Version, Mode, Capacity, Creator PID  |
+---------------------------------------------------------------------------------------+
|  [Cache Line 2]     _Alignas(64) atomic_uint_fast64_t write_cursor (Producer Cursor)  |
+---------------------------------------------------------------------------------------+
|  [Cache Line 3]     _Alignas(64) atomic_uint_fast64_t read_cursor  (Consumer Cursor)  |
+---------------------------------------------------------------------------------------+
|  [Cache Line 4]     _Alignas(64) atomic_uint_fast32_t state_flags & waiting counters  |
+---------------------------------------------------------------------------------------+
|  [Cache Line 5 ~ N] Ring Data Area:                                                   |
|                     SPSC: Contiguous byte ring with wrap-around padding support       |
|                     MPMC: Array of slots (each with sequence + payload)               |
+---------------------------------------------------------------------------------------+
```

### Header Struct Definition

```c
#define COBALT_SHM_MAGIC       0x434F425F53484D31ULL /* "COB_SHM1" */
#define COBALT_SHM_VERSION     1

typedef enum {
    COBALT_SHM_MODE_SPSC = 1,
    COBALT_SHM_MODE_MPMC = 2
} cobalt_shm_mode_t;

typedef struct {
    /* Cache Line 0: Static Metadata */
    uint64_t magic;
    uint32_t version;
    uint32_t mode;
    uint64_t total_shm_size;
    uint64_t capacity;        /* Must be a power of two */
    uint32_t slot_size;       /* Size of single slot for MPMC; 0 for SPSC */
    uint32_t creator_pid;

    /* Cache Line 1: Heartbeat & Reserved */
    _Alignas(64) atomic_uint_fast64_t heartbeat;
    uint8_t reserved[56];

    /* Cache Line 2: Producer Write Cursor (Isolated cache line) */
    _Alignas(64) atomic_uint_fast64_t write_cursor;

    /* Cache Line 3: Consumer Read Cursor (Isolated cache line) */
    _Alignas(64) atomic_uint_fast64_t read_cursor;

    /* Cache Line 4: Synchronization & Waiter Tracking */
    _Alignas(64) atomic_uint_fast32_t channel_state;
    atomic_uint_fast32_t waiting_readers;
    atomic_uint_fast32_t waiting_writers;
    uint8_t state_padding[52];
} cobalt_shm_header_t;
```

---

## 4. Concurrency & Lock-Free Protocols

### 4.1 SPSC: Variable-Length Zero-Copy Protocol

#### Framing Format
```c
#define COBALT_SHM_FRAME_NORMAL 0x0000
#define COBALT_SHM_FRAME_WRAP   0x0001 /* Tail padding: reader wraps to offset 0 */

typedef struct {
    uint32_t length;     /* Payload byte length */
    uint16_t type;       /* User-defined message type */
    uint16_t flags;      /* Frame flags (COBALT_SHM_FRAME_*) */
} cobalt_shm_frame_t;
```

#### Zero-Copy Reserve / Commit Flow:
1. **Reserve**:
   - Producer checks remaining contiguous bytes at the tail of the ring: `tail_space = capacity - (write_pos & mask)`.
   - If `tail_space < frame_size` and space at ring start is available:
     - Write a frame with `flags = COBALT_SHM_FRAME_WRAP`.
     - Advance `write_cursor` by `tail_space`.
   - Read `read_cursor` with `memory_order_acquire` to ensure `write_cursor - read_cursor + frame_size <= capacity`.
   - Direct pointer `*out_ptr = buffer + (write_cursor & mask) + sizeof(cobalt_shm_frame_t)` is returned to the user.
2. **Commit**:
   - Write frame header (`length`, `type`, `flags = 0`).
   - Store `write_cursor` with `memory_order_release`.
   - **Zero CAS operations**: Entire path requires only 1 acquire load and 1 release store.

#### Zero-Copy Peek / Consume Flow:
1. **Peek**:
   - Read `write_cursor` with `memory_order_acquire`.
   - If `write_cursor == read_cursor`: Ring is empty.
   - Inspect frame header at `read_cursor & mask`.
   - If `flags & COBALT_SHM_FRAME_WRAP`: advance `read_cursor` past the wrap padding and reload frame at offset 0.
   - Return payload pointer `*out_ptr` and message metadata.
2. **Consume**:
   - Advance `read_cursor` past the consumed frame with `memory_order_release`.

---

### 4.2 MPMC: Dmitry Vyukov Sequence-Slot Protocol

```c
typedef struct {
    _Alignas(64) atomic_uint_fast64_t sequence;
    uint8_t payload[]; /* slot_size bytes */
} cobalt_shm_slot_t;
```

1. **Initialization**: Each slot $i$ has `sequence` initialized to $i$.
2. **Enqueue (Multi-Producer)**:
   - Loop:
     - `pos = atomic_load(write_cursor)`.
     - `slot = slots[pos & mask]`.
     - `seq = atomic_load_explicit(&slot->sequence, memory_order_acquire)`.
     - `diff = (int64_t)seq - (int64_t)pos`.
     - If `diff == 0`: Try CAS `write_cursor` from `pos` to `pos + 1`. If won:
       - `memcpy(slot->payload, data, size)`.
       - `atomic_store_explicit(&slot->sequence, pos + 1, memory_order_release)`. Return `COBALT_SUCCESS`.
     - If `diff < 0`: Ring full.
3. **Dequeue (Multi-Consumer)**:
   - Symmetric logic checking `seq == pos + 1`, CAS `read_cursor`, and updating `slot->sequence = pos + capacity`.

---

## 5. Waiting & EventLoop Integration

### Wait Policy
```c
typedef enum {
    COBALT_SHM_WAIT_SPIN = 0,     /* 100% spin, zero syscalls (<30ns) */
    COBALT_SHM_WAIT_BLOCKING = 1, /* Direct OS descriptor sleep */
    COBALT_SHM_WAIT_ADAPTIVE = 2  /* Spin threshold (e.g. 1000 spins) then OS sleep */
} cobalt_shm_wait_policy_t;
```

### EventLoop Integration API
```c
int cobalt_shm_channel_get_read_fd(const cobalt_shm_channel_t *chan);

typedef void (*cobalt_shm_msg_cb)(
    cobalt_shm_channel_t *chan,
    uint16_t msg_type,
    const void *payload,
    size_t len,
    void *user_data
);

int cobalt_shm_channel_attach_eventloop(
    cobalt_shm_channel_t *chan,
    cobalt_eventloop_t *loop,
    cobalt_shm_msg_cb cb,
    void *user_data
);
```

When an event arrives, the callback performs a **batch drain** loop:
```c
while (cobalt_shm_channel_peek(chan, &type, &ptr, &len) == COBALT_SUCCESS) {
    cb(chan, type, ptr, len, user_data);
    cobalt_shm_channel_consume(chan);
}
```

---

## 6. Public API Declarations

### `include/cobalt/platform/shm.h` (Layer 8)
- `int cobalt_shm_create(const char *name, size_t size, cobalt_shm_t **out_shm);`
- `int cobalt_shm_open(const char *name, cobalt_shm_t **out_shm);`
- `void *cobalt_shm_get_addr(cobalt_shm_t *shm);`
- `size_t cobalt_shm_get_size(const cobalt_shm_t *shm);`
- `void cobalt_shm_close(cobalt_shm_t *shm);`
- `int cobalt_shm_unlink(const char *name);`

### `include/cobalt/platform/notifier.h` (Layer 8)
- `int cobalt_notifier_create(const char *name, cobalt_notifier_t **out_notif);`
- `int cobalt_notifier_open(const char *name, cobalt_notifier_t **out_notif);`
- `int cobalt_notifier_notify(cobalt_notifier_t *notif);`
- `int cobalt_notifier_wait(cobalt_notifier_t *notif, int timeout_ms);`
- `int cobalt_notifier_get_fd(const cobalt_notifier_t *notif);`
- `void cobalt_notifier_destroy(cobalt_notifier_t *notif);`

### `include/cobalt/module/shm_channel.h` (Layer 2)
- `cobalt_shm_channel_t *cobalt_shm_channel_create(const cobalt_shm_config_t *config);`
- `cobalt_shm_channel_t *cobalt_shm_channel_open(const char *name, cobalt_shm_wait_policy_t policy);`
- `void cobalt_shm_channel_destroy(cobalt_shm_channel_t *chan);`
- `int cobalt_shm_channel_reserve(cobalt_shm_channel_t *chan, size_t len, void **out_ptr);`
- `int cobalt_shm_channel_commit(cobalt_shm_channel_t *chan, uint16_t msg_type, size_t len);`
- `int cobalt_shm_channel_peek(cobalt_shm_channel_t *chan, uint16_t *out_type, const void **out_ptr, size_t *out_len);`
- `int cobalt_shm_channel_consume(cobalt_shm_channel_t *chan);`
- `int cobalt_shm_channel_send(cobalt_shm_channel_t *chan, uint16_t msg_type, const void *data, size_t len, int timeout_ms);`
- `int cobalt_shm_channel_recv(cobalt_shm_channel_t *chan, uint16_t *out_type, void *buf, size_t buf_size, size_t *out_len, int timeout_ms);`

---

## 7. Verification & Testing Matrix

### Multi-Process Testing Strategy (`tests/unit/test_shm.c`)
- POSIX `fork()` harness: Parent acts as owner/server, child attaches as client.
- 7 comprehensive test suites:
  1. `test_shm_spsc_basic`: Zero-copy reserve/commit and peek/consume.
  2. `test_shm_spsc_wraparound`: Ring boundary handling and wrap frame markers.
  3. `test_shm_spsc_perf_loop`: 100,000 message ping-pong with CRC32 verification.
  4. `test_shm_mpmc_contention`: 4 producers + 4 consumers transferring 500,000 messages concurrently.
  5. `test_shm_eventloop_drain`: 20,000 messages dispatched to `cobalt_eventloop_t` with batch draining.
  6. `test_shm_wait_policies`: Verifying CPU usage and signaling across SPIN / BLOCKING / ADAPTIVE.
  7. `test_shm_orphan_cleanup`: Simulating ungraceful process crash (`SIGKILL`) and verifying safe recovery/unlinking.

### Benchmark Suite (`tests/benchmark/benchmark_shm.c`)
- Round-Trip Latency (RTT) distribution: Min, Median, P99 in nanoseconds.
- Throughput across payloads: 64B, 1KB, 64KB (M ops/s and GB/s).
- Target Metrics:
  - RTT $\le$ 80ns (Spin)
  - Throughput $\ge$ 20M msgs/sec
  - Speedup vs Unix Domain Socket / Pipe: 30x~50x lower latency.
