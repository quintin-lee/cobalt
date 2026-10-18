/**
 * @file test_buffer.c
 * @brief Unit tests for Layer 4 dynamic zero-copy stream buffer
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "cobalt/container/buffer.h"
#include "cobalt/memory/allocator.h"
#include "test_framework.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void test_buffer_create_destroy(void)
{
    printf("Testing buffer create and destroy...\n");

    cobalt_buffer_t *buf = cobalt_buffer_create(0);
    TEST_ASSERT(buf != NULL);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), 0U);
    TEST_EQUAL(cobalt_buffer_writable_bytes(buf), (size_t)COBALT_BUFFER_DEFAULT_INITIAL_CAPACITY);
    TEST_EQUAL(cobalt_buffer_prependable_bytes(buf), (size_t)COBALT_BUFFER_DEFAULT_PREPEND_SIZE);
    TEST_ASSERT(cobalt_buffer_peek(buf) == NULL);

    cobalt_buffer_destroy(buf);
    cobalt_buffer_destroy(NULL); /* Should not crash */
    printf("  buffer create/destroy: OK\n");
}

static void test_buffer_append_read_retrieve(void)
{
    printf("Testing buffer append, read, and retrieve...\n");

    cobalt_buffer_t *buf = cobalt_buffer_create(64);
    TEST_ASSERT(buf != NULL);

    const char *msg = "Hello, Cobalt Stream Buffer!";
    size_t      len = strlen(msg);

    TEST_EQUAL(cobalt_buffer_append(buf, msg, len), 0);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), len);
    TEST_ASSERT(memcmp(cobalt_buffer_peek(buf), msg, len) == 0);

    /* Partial read */
    char part[8];
    TEST_EQUAL(cobalt_buffer_read(buf, part, 7), 0);
    part[7] = '\0';
    TEST_ASSERT(strcmp(part, "Hello, ") == 0);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), len - 7);

    /* Retrieve a few more bytes */
    TEST_EQUAL(cobalt_buffer_retrieve(buf, 7), 0);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), len - 14);

    /* Over-retrieve check */
    TEST_EQUAL(cobalt_buffer_retrieve(buf, 9999), -1);

    /* Retrieve all */
    cobalt_buffer_retrieve_all(buf);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), 0U);
    TEST_ASSERT(cobalt_buffer_peek(buf) == NULL);

    cobalt_buffer_destroy(buf);
    printf("  buffer append/read/retrieve: OK\n");
}

static void test_buffer_prepend(void)
{
    printf("Testing buffer prependable header space...\n");

    cobalt_buffer_t *buf = cobalt_buffer_create_with_alloc(64, 8, NULL);
    TEST_ASSERT(buf != NULL);
    TEST_EQUAL(cobalt_buffer_prependable_bytes(buf), 8U);

    const char *payload = "PAYLOAD_BODY";
    size_t      plen    = strlen(payload);
    TEST_EQUAL(cobalt_buffer_append(buf, payload, plen), 0);

    /* Prepend 4-byte length prefix */
    uint32_t header = 0x12345678;
    TEST_EQUAL(cobalt_buffer_prepend(buf, &header, sizeof(header)), 0);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), plen + sizeof(header));
    TEST_EQUAL(cobalt_buffer_prependable_bytes(buf), 8U - sizeof(header));

    /* Prepend another 4 bytes (exactly exhausts 8 bytes) */
    uint32_t magic = 0xCAFEBABE;
    TEST_EQUAL(cobalt_buffer_prepend(buf, &magic, sizeof(magic)), 0);
    TEST_EQUAL(cobalt_buffer_prependable_bytes(buf), 0U);

    /* Further prepend should fail */
    TEST_EQUAL(cobalt_buffer_prepend(buf, &magic, 1), -1);

    /* Validate full combined content */
    const char *peeked = cobalt_buffer_peek(buf);
    TEST_ASSERT(memcmp(peeked, &magic, 4) == 0);
    TEST_ASSERT(memcmp(peeked + 4, &header, 4) == 0);
    TEST_ASSERT(memcmp(peeked + 8, payload, plen) == 0);

    cobalt_buffer_destroy(buf);
    printf("  buffer prepend: OK\n");
}

static void test_buffer_defragment_and_expand(void)
{
    printf("Testing buffer defragmentation and expansion...\n");

    cobalt_buffer_t *buf = cobalt_buffer_create_with_alloc(128, 8, NULL);
    TEST_ASSERT(buf != NULL);

    /* Fill buffer with 100 bytes */
    char chunk[100];
    memset(chunk, 'A', sizeof(chunk));
    TEST_EQUAL(cobalt_buffer_append(buf, chunk, 100), 0);

    /* Consume 80 bytes, leaving 20 bytes readable */
    TEST_EQUAL(cobalt_buffer_retrieve(buf, 80), 0);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), 20U);
    TEST_ASSERT(cobalt_buffer_prependable_bytes(buf) == 88U);

    /* Writable was 136 - 108 = 28 bytes. Now request 90 bytes.
       28 + 80 discardable = 108 >= 90, so in-place defragmentation should trigger! */
    char chunk2[90];
    memset(chunk2, 'B', sizeof(chunk2));
    TEST_EQUAL(cobalt_buffer_append(buf, chunk2, 90), 0);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), 110U);

    /* Verify the first 20 bytes are still 'A' and next 90 bytes are 'B' */
    const char *data = cobalt_buffer_peek(buf);
    for (size_t i = 0; i < 20; i++) {
        TEST_ASSERT(data[i] == 'A');
    }
    for (size_t i = 20; i < 110; i++) {
        TEST_ASSERT(data[i] == 'B');
    }

    /* Massive append triggering geometric expansion */
    const size_t big_len = 100000;
    char        *big_buf = (char *)malloc(big_len);
    TEST_ASSERT(big_buf != NULL);
    memset(big_buf, 'Z', big_len);

    TEST_EQUAL(cobalt_buffer_append(buf, big_buf, big_len), 0);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), 110U + big_len);

    free(big_buf);
    cobalt_buffer_destroy(buf);
    printf("  buffer defragment/expand: OK\n");
}

/**
 * @brief Regression: defragmentation must not underflow when read_pos dropped
 *        below prepend_size via prepend().
 *
 *        prepend() moves read_pos down toward 0 while prepend_size stays fixed.
 *        A subsequent full append makes the defragmentation branch compute
 *        `read_pos - prepend_size`, which underflows to SIZE_MAX-8 on a
 *        40-byte capacity buffer and caused a 40-byte heap out-of-bounds write
 *        (ASan-confirmed). The fix clamps discardable to 0 and routes to the
 *        geometric expansion branch instead.
 */
static void test_buffer_defrag_prepend_no_underflow(void)
{
    printf("Testing buffer defrag guard after prepend (regression)...\n");

    /* capacity = 32 + 8 = 40 bytes total */
    cobalt_buffer_t *buf = cobalt_buffer_create_with_alloc(32, 8, NULL);
    TEST_ASSERT(buf != NULL);

    char p[8];
    memset(p, 0xAB, sizeof(p));
    TEST_EQUAL(cobalt_buffer_prepend(buf, p, 8), 0);
    /* read_pos is now 0, below prepend_size (8) */

    char q[32];
    memset(q, 'Z', sizeof(q));
    TEST_EQUAL(cobalt_buffer_append(buf, q, 32), 0);

    /* Force the expansion/defrag path: 1 more byte must be writable */
    TEST_EQUAL(cobalt_buffer_ensure_writable(buf, 1), 0);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), 40U);

    /* Content must be intact: 8 bytes of p then 32 bytes of q */
    const char *data = cobalt_buffer_peek(buf);
    TEST_ASSERT(memcmp(data, p, 8) == 0);
    TEST_ASSERT(memcmp(data + 8, q, 32) == 0);

    cobalt_buffer_destroy(buf);
    printf("  buffer defrag guard: OK\n");
}
#ifndef _WIN32
static void test_buffer_read_write_fd(void)
{
    printf("Testing buffer vectorized read_fd and write_fd with pipe...\n");

    int pipe1[2];
    int pipe2[2];
    TEST_ASSERT(pipe(pipe1) == 0);
    TEST_ASSERT(pipe(pipe2) == 0);

    /* Set non-blocking */
    int flags = fcntl(pipe1[0], F_GETFL, 0);
    fcntl(pipe1[0], F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(pipe2[1], F_GETFL, 0);
    fcntl(pipe2[1], F_SETFL, flags | O_NONBLOCK);

    /* Write 32KB into pipe1 */
    const size_t test_size = 32768;
    char        *src       = (char *)malloc(test_size);
    TEST_ASSERT(src != NULL);
    for (size_t i = 0; i < test_size; i++) {
        src[i] = (char)(i & 0xFF);
    }
    TEST_EQUAL(write(pipe1[1], src, test_size), (ssize_t)test_size);

    /* Initial small buffer (256 bytes) to exercise the 64KB stack buffer overflow path */
    cobalt_buffer_t *buf = cobalt_buffer_create(256);
    TEST_ASSERT(buf != NULL);

    int     err = 0;
    ssize_t n   = cobalt_buffer_read_fd(buf, pipe1[0], &err);
    TEST_EQUAL(n, (ssize_t)test_size);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), test_size);

    /* Write out to pipe2 */
    ssize_t written = cobalt_buffer_write_fd(buf, pipe2[1], &err);
    TEST_EQUAL(written, (ssize_t)test_size);
    TEST_EQUAL(cobalt_buffer_readable_bytes(buf), 0U);

    /* Verify pipe2 received correct bytes */
    char *dst = (char *)malloc(test_size);
    TEST_ASSERT(dst != NULL);
    TEST_EQUAL(read(pipe2[0], dst, test_size), (ssize_t)test_size);
    TEST_ASSERT(memcmp(src, dst, test_size) == 0);

    free(src);
    free(dst);
    cobalt_buffer_destroy(buf);
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);
    printf("  buffer read_fd/write_fd: OK\n");
}
#endif

/* Custom mock allocator test */
static size_t g_mock_alloc_count = 0;
static size_t g_mock_free_count  = 0;

static void *mock_alloc(cobalt_allocator_t *self, size_t size)
{
    (void)self;
    g_mock_alloc_count++;
    return malloc(size);
}

static void mock_free(cobalt_allocator_t *self, void *ptr)
{
    (void)self;
    if (ptr) {
        g_mock_free_count++;
        free(ptr);
    }
}

static void *mock_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
{
    (void)self;
    return realloc(ptr, new_size);
}

static void test_buffer_custom_alloc(void)
{
    printf("Testing buffer with custom allocator injection...\n");

    cobalt_allocator_t custom_alloc = {
        .alloc   = mock_alloc,
        .free    = mock_free,
        .realloc = mock_realloc,
    };

    g_mock_alloc_count = 0;
    g_mock_free_count  = 0;

    cobalt_buffer_t *buf = cobalt_buffer_create_with_alloc(256, 16, &custom_alloc);
    TEST_ASSERT(buf != NULL);
    TEST_ASSERT(g_mock_alloc_count == 2); /* 1 for struct, 1 for data */

    cobalt_buffer_append(buf, "test_custom_alloc", 17);
    cobalt_buffer_destroy(buf);

    TEST_ASSERT(g_mock_free_count == 2); /* All freed */
    printf("  buffer custom allocator: OK\n");
}

void test_buffer(void)
{
    printf("=== Cobalt Unit Test: buffer ===\n\n");
    test_buffer_create_destroy();
    test_buffer_append_read_retrieve();
    test_buffer_prepend();
    test_buffer_defragment_and_expand();
    test_buffer_defrag_prepend_no_underflow();
#ifndef _WIN32
    test_buffer_read_write_fd();
#endif
    test_buffer_custom_alloc();
    printf("  Buffer tests completed\n");
}
