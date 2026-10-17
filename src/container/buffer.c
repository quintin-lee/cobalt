/**
 * @file buffer.c
 * @brief Implementation of dynamic zero-copy contiguous stream buffer
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "cobalt/container/buffer.h"
#include "cobalt/memory/allocator.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

struct cobalt_buffer {
    char               *data;
    size_t              capacity;
    size_t              read_pos;
    size_t              write_pos;
    size_t              prepend_size;
    cobalt_allocator_t *alloc;
};

cobalt_buffer_t *cobalt_buffer_create(size_t initial_capacity)
{
    return cobalt_buffer_create_with_alloc(
        initial_capacity, COBALT_BUFFER_DEFAULT_PREPEND_SIZE, cobalt_allocator_get_system());
}

cobalt_buffer_t *cobalt_buffer_create_with_alloc(size_t              initial_capacity,
                                                 size_t              prepend_size,
                                                 cobalt_allocator_t *alloc)
{
    if (initial_capacity == 0) {
        initial_capacity = COBALT_BUFFER_DEFAULT_INITIAL_CAPACITY;
    }
    if (prepend_size == 0) {
        prepend_size = COBALT_BUFFER_DEFAULT_PREPEND_SIZE;
    }
    if (!alloc) {
        alloc = cobalt_allocator_get_system();
    }

    cobalt_buffer_t *buf = (cobalt_buffer_t *)alloc->alloc(alloc, sizeof(cobalt_buffer_t));
    if (!buf) {
        return NULL;
    }

    size_t total_cap = initial_capacity + prepend_size;
    buf->data        = (char *)alloc->alloc(alloc, total_cap);
    if (!buf->data) {
        alloc->free(alloc, buf);
        return NULL;
    }

    buf->capacity     = total_cap;
    buf->read_pos     = prepend_size;
    buf->write_pos    = prepend_size;
    buf->prepend_size = prepend_size;
    buf->alloc        = alloc;
    return buf;
}

void cobalt_buffer_destroy(cobalt_buffer_t *buf)
{
    if (!buf) {
        return;
    }
    cobalt_allocator_t *alloc = buf->alloc;
    if (buf->data) {
        alloc->free(alloc, buf->data);
        buf->data = NULL;
    }
    alloc->free(alloc, buf);
}

size_t cobalt_buffer_readable_bytes(const cobalt_buffer_t *buf)
{
    return buf ? (buf->write_pos - buf->read_pos) : 0;
}

size_t cobalt_buffer_writable_bytes(const cobalt_buffer_t *buf)
{
    return buf ? (buf->capacity - buf->write_pos) : 0;
}

size_t cobalt_buffer_prependable_bytes(const cobalt_buffer_t *buf)
{
    return buf ? buf->read_pos : 0;
}

const char *cobalt_buffer_peek(const cobalt_buffer_t *buf)
{
    return (buf && buf->write_pos > buf->read_pos) ? (buf->data + buf->read_pos) : NULL;
}

int cobalt_buffer_retrieve(cobalt_buffer_t *buf, size_t len)
{
    if (!buf || len > cobalt_buffer_readable_bytes(buf)) {
        return -1;
    }
    if (len < cobalt_buffer_readable_bytes(buf)) {
        buf->read_pos += len;
    } else {
        cobalt_buffer_retrieve_all(buf);
    }
    return 0;
}

void cobalt_buffer_retrieve_all(cobalt_buffer_t *buf)
{
    if (!buf) {
        return;
    }
    buf->read_pos  = buf->prepend_size;
    buf->write_pos = buf->prepend_size;
}

int cobalt_buffer_read(cobalt_buffer_t *buf, void *dst, size_t len)
{
    if (!buf || !dst || len > cobalt_buffer_readable_bytes(buf)) {
        return -1;
    }
    memcpy(dst, buf->data + buf->read_pos, len);
    cobalt_buffer_retrieve(buf, len);
    return 0;
}

int cobalt_buffer_ensure_writable(cobalt_buffer_t *buf, size_t len)
{
    if (!buf) {
        return -1;
    }
    if (cobalt_buffer_writable_bytes(buf) >= len) {
        return 0;
    }

    /* Check if in-place defragmentation satisfies demand */
    size_t readable    = cobalt_buffer_readable_bytes(buf);
    size_t discardable = buf->read_pos - buf->prepend_size;
    if (discardable + cobalt_buffer_writable_bytes(buf) >= len) {
        memmove(buf->data + buf->prepend_size, buf->data + buf->read_pos, readable);
        buf->read_pos  = buf->prepend_size;
        buf->write_pos = buf->prepend_size + readable;
        return 0;
    }

    /* Need geometric expansion */
    size_t new_cap = buf->capacity * 2;
    size_t needed  = buf->write_pos + len;
    if (new_cap < needed) {
        new_cap = needed + buf->capacity;
    }

    char *new_data = NULL;
    if (buf->alloc->realloc) {
        new_data = (char *)buf->alloc->realloc(buf->alloc, buf->data, new_cap);
    } else {
        new_data = (char *)buf->alloc->alloc(buf->alloc, new_cap);
        if (new_data) {
            memcpy(new_data, buf->data, buf->capacity);
            buf->alloc->free(buf->alloc, buf->data);
        }
    }

    if (!new_data) {
        return -1;
    }

    buf->data     = new_data;
    buf->capacity = new_cap;
    return 0;
}

int cobalt_buffer_append(cobalt_buffer_t *buf, const void *data, size_t len)
{
    if (!buf || (len > 0 && !data)) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (cobalt_buffer_ensure_writable(buf, len) != 0) {
        return -1;
    }
    memcpy(buf->data + buf->write_pos, data, len);
    buf->write_pos += len;
    return 0;
}

int cobalt_buffer_prepend(cobalt_buffer_t *buf, const void *data, size_t len)
{
    if (!buf || (len > 0 && !data)) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (len > buf->read_pos) {
        return -1;
    }
    buf->read_pos -= len;
    memcpy(buf->data + buf->read_pos, data, len);
    return 0;
}

ssize_t cobalt_buffer_read_fd(cobalt_buffer_t *buf, int fd, int *out_err)
{
    if (!buf || fd < 0) {
        if (out_err) {
            *out_err = EINVAL;
        }
        return -1;
    }

    char         stack_buf[65536];
    struct iovec iov[2];
    size_t       writable = cobalt_buffer_writable_bytes(buf);

    iov[0].iov_base = buf->data + buf->write_pos;
    iov[0].iov_len  = writable;
    iov[1].iov_base = stack_buf;
    iov[1].iov_len  = sizeof(stack_buf);

    const int iovcnt = (writable < sizeof(stack_buf)) ? 2 : 1;
    ssize_t   n      = readv(fd, iov, iovcnt);
    if (n < 0) {
        if (out_err) {
            *out_err = errno;
        }
        return -1;
    }

    if ((size_t)n <= writable) {
        buf->write_pos += (size_t)n;
    } else {
        buf->write_pos  = buf->capacity;
        size_t overflow = (size_t)n - writable;
        if (cobalt_buffer_append(buf, stack_buf, overflow) != 0) {
            if (out_err) {
                *out_err = ENOMEM;
            }
            return -1;
        }
    }
    return n;
}

ssize_t cobalt_buffer_write_fd(cobalt_buffer_t *buf, int fd, int *out_err)
{
    if (!buf || fd < 0) {
        if (out_err) {
            *out_err = EINVAL;
        }
        return -1;
    }

    size_t readable = cobalt_buffer_readable_bytes(buf);
    if (readable == 0) {
        return 0;
    }

    ssize_t n = write(fd, buf->data + buf->read_pos, readable);
    if (n < 0) {
        if (out_err) {
            *out_err = errno;
        }
        return -1;
    }

    cobalt_buffer_retrieve(buf, (size_t)n);
    return n;
}
