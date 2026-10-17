#ifndef COBALT_CONTAINER_BUFFER_H
#define COBALT_CONTAINER_BUFFER_H

/**
 * @file buffer.h
 * @brief Dynamic zero-copy contiguous stream buffer
 * @details Three-zone buffer (prependable, readable, writable) inspired by Netty ByteBuf
 *          and Muduo Buffer, featuring in-place defragmentation, prepended framing headers,
 *          and two-segment vectorized readv/writev integration.
 *
 * @defgroup Container_Buffer Stream Buffer
 * @ingroup Container
 * @{
 */

#include "cobalt/memory/allocator.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COBALT_BUFFER_DEFAULT_INITIAL_CAPACITY 1024
#define COBALT_BUFFER_DEFAULT_PREPEND_SIZE 8

/**
 * @brief Opaque stream buffer structure
 */
typedef struct cobalt_buffer cobalt_buffer_t;

/**
 * @brief Create a new stream buffer with default settings and system allocator
 * @param initial_capacity Initial writable capacity in bytes (0 for default 1024)
 * @return Newly created buffer, or NULL on allocation failure
 */
cobalt_buffer_t *cobalt_buffer_create(size_t initial_capacity);

/**
 * @brief Create a new stream buffer with custom prepend size and allocator
 * @param initial_capacity Initial capacity in bytes
 * @param prepend_size Bytes reserved at front for framing headers (0 for default 8)
 * @param alloc Custom allocator instance (NULL for system allocator)
 * @return Newly created buffer, or NULL on allocation failure
 */
cobalt_buffer_t *cobalt_buffer_create_with_alloc(size_t              initial_capacity,
                                                 size_t              prepend_size,
                                                 cobalt_allocator_t *alloc);

/**
 * @brief Destroy a stream buffer and release associated resources
 * @param buf Buffer to destroy. No-op if NULL.
 */
void cobalt_buffer_destroy(cobalt_buffer_t *buf);

/**
 * @brief Get the number of readable bytes currently in the buffer
 * @param buf Stream buffer
 * @return Number of readable bytes (write_pos - read_pos)
 */
size_t cobalt_buffer_readable_bytes(const cobalt_buffer_t *buf);

/**
 * @brief Get the number of writable bytes currently available without resizing
 * @param buf Stream buffer
 * @return Number of writable bytes (capacity - write_pos)
 */
size_t cobalt_buffer_writable_bytes(const cobalt_buffer_t *buf);

/**
 * @brief Get the number of prependable bytes reserved or freed before read_pos
 * @param buf Stream buffer
 * @return Prependable byte count (read_pos)
 */
size_t cobalt_buffer_prependable_bytes(const cobalt_buffer_t *buf);

/**
 * @brief Direct read-only pointer to readable payload (zero-copy inspection)
 * @param buf Stream buffer
 * @return Pointer to current read cursor, or NULL if empty
 */
const char *cobalt_buffer_peek(const cobalt_buffer_t *buf);

/**
 * @brief Consume and advance read cursor by len bytes
 * @param buf Stream buffer
 * @param len Number of bytes to consume
 * @return 0 on success, -1 if len > readable bytes
 */
int cobalt_buffer_retrieve(cobalt_buffer_t *buf, size_t len);

/**
 * @brief Reset buffer to initial empty state, retaining allocated memory
 * @param buf Stream buffer
 */
void cobalt_buffer_retrieve_all(cobalt_buffer_t *buf);

/**
 * @brief Read and copy bytes into destination, advancing read cursor
 * @param buf Stream buffer
 * @param dst Destination memory buffer
 * @param len Number of bytes to read
 * @return 0 on success, -1 if len > readable bytes
 */
int cobalt_buffer_read(cobalt_buffer_t *buf, void *dst, size_t len);

/**
 * @brief Ensure at least len writable bytes are available, defragmenting or expanding if needed
 * @param buf Stream buffer
 * @param len Desired writable space in bytes
 * @return 0 on success, -1 on allocation failure
 */
int cobalt_buffer_ensure_writable(cobalt_buffer_t *buf, size_t len);

/**
 * @brief Append bytes to the end of readable payload
 * @param buf Stream buffer
 * @param data Data pointer to append
 * @param len Number of bytes to append
 * @return 0 on success, -1 on failure
 */
int cobalt_buffer_append(cobalt_buffer_t *buf, const void *data, size_t len);

/**
 * @brief Prepend bytes into the reserved header space before read_pos
 * @param buf Stream buffer
 * @param data Data pointer to prepend
 * @param len Number of bytes to prepend
 * @return 0 on success, -1 if len > prependable bytes
 */
int cobalt_buffer_prepend(cobalt_buffer_t *buf, const void *data, size_t len);

/**
 * @brief Read bytes from non-blocking file descriptor into buffer via two-segment readv
 * @details Reads up to buffer writable capacity into internal memory, and spills up to 64KB
 *          into a stack buffer, expanding dynamically only if data exceeded capacity.
 * @param buf Stream buffer
 * @param fd File descriptor to read from
 * @param out_err Optional pointer to receive errno on failure
 * @return Bytes read (>=0), or -1 on error
 */
ssize_t cobalt_buffer_read_fd(cobalt_buffer_t *buf, int fd, int *out_err);

/**
 * @brief Write readable bytes to non-blocking file descriptor, consuming written bytes
 * @param buf Stream buffer
 * @param fd File descriptor to write to
 * @param out_err Optional pointer to receive errno on failure
 * @return Bytes written (>=0), or -1 on error
 */
ssize_t cobalt_buffer_write_fd(cobalt_buffer_t *buf, int fd, int *out_err);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* COBALT_CONTAINER_BUFFER_H */
