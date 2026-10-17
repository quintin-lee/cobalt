/**
 * @file test_shm.c
 * @brief Unit tests for platform shared memory primitive (cobalt_shm_t).
 */

#include "cobalt/container/shm_ring.h"
#include "cobalt/platform/notifier.h"
#include "cobalt/platform/shm.h"
#include "cobalt/platform/thread.h"
#include "cobalt/runtime/error.h"
#include "test_framework.h"

#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_shm_create_and_close(void)
{
    printf("Testing cobalt_shm_create and cobalt_shm_close...\n");

    const char *name = "/cobalt_test_shm_create";
    /* Clean up any remnant */
    cobalt_shm_unlink(name);

    cobalt_shm_t *shm = NULL;
    int           rc  = cobalt_shm_create(name, 4096, &shm);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(shm != NULL);

    void *addr = cobalt_shm_get_addr(shm);
    TEST_ASSERT(addr != NULL);

    size_t size = cobalt_shm_get_size(shm);
    TEST_ASSERT(size == 4096);

    /* Write pattern and verify */
    memset(addr, 0xAB, 4096);
    unsigned char *bytes = (unsigned char *)addr;
    TEST_ASSERT(bytes[0] == 0xAB);
    TEST_ASSERT(bytes[2048] == 0xAB);
    TEST_ASSERT(bytes[4095] == 0xAB);

    cobalt_shm_close(shm);

    rc = cobalt_shm_unlink(name);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    printf("  Create and close test passed\n");
}

static void test_shm_open_existing(void)
{
    printf("Testing cobalt_shm_open existing shared memory...\n");

    const char *name = "/cobalt_test_shm_open";
    cobalt_shm_unlink(name);

    cobalt_shm_t *shm_a = NULL;
    int           rc    = cobalt_shm_create(name, 4096, &shm_a);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(shm_a != NULL);

    cobalt_shm_t *shm_dup = NULL;
    int           dup_rc  = cobalt_shm_create(name, 4096, &shm_dup);
    TEST_ASSERT(dup_rc == COBALT_ERROR_ALREADY_EXISTS);
    TEST_ASSERT(shm_dup == NULL);

    uint32_t *magic_a = (uint32_t *)cobalt_shm_get_addr(shm_a);
    TEST_ASSERT(magic_a != NULL);
    *magic_a = 0xDEADBEEF;

    cobalt_shm_t *shm_b = NULL;
    rc                  = cobalt_shm_open(name, &shm_b);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(shm_b != NULL);
    TEST_ASSERT(cobalt_shm_get_size(shm_b) == 4096);

    uint32_t *magic_b = (uint32_t *)cobalt_shm_get_addr(shm_b);
    TEST_ASSERT(magic_b != NULL);
    TEST_ASSERT(*magic_b == 0xDEADBEEF);

    /* Write from B, observe in A */
    *magic_b = 0xCAFEBABE;
    TEST_ASSERT(*magic_a == 0xCAFEBABE);

    cobalt_shm_close(shm_b);
    cobalt_shm_close(shm_a);

    rc = cobalt_shm_unlink(name);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    printf("  Open existing test passed\n");
}

static void test_shm_invalid_args(void)
{
    printf("Testing cobalt_shm invalid arguments...\n");

    cobalt_shm_t *shm = NULL;

    /* create: NULL name */
    TEST_ASSERT(cobalt_shm_create(NULL, 4096, &shm) == COBALT_ERROR_INVALID_ARGUMENT);

    /* create: size == 0 */
    TEST_ASSERT(cobalt_shm_create("/cobalt_test_shm_zero", 0, &shm) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* create: NULL out_shm */
    TEST_ASSERT(cobalt_shm_create("/cobalt_test_shm_null_out", 4096, NULL) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* open: NULL name */
    TEST_ASSERT(cobalt_shm_open(NULL, &shm) == COBALT_ERROR_INVALID_ARGUMENT);

    /* open: NULL out_shm */
    TEST_ASSERT(cobalt_shm_open("/cobalt_test_shm_null_out", NULL) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* open: non-existent name */
    TEST_ASSERT(cobalt_shm_open("/cobalt_test_nonexistent_xyz_9999", &shm) ==
                COBALT_ERROR_NOT_FOUND);

    /* unlink: NULL name */
    TEST_ASSERT(cobalt_shm_unlink(NULL) == COBALT_ERROR_INVALID_ARGUMENT);

    /* close: NULL handle is safe no-op */
    cobalt_shm_close(NULL);

    /* getters on NULL handle */
    TEST_ASSERT(cobalt_shm_get_addr(NULL) == NULL);
    TEST_ASSERT(cobalt_shm_get_size(NULL) == 0);

    printf("  Invalid arguments test passed\n");
}

static void test_shm_name_formatting(void)
{
    printf("Testing shared memory name without leading slash...\n");

    const char *name = "cobalt_test_shm_noslash";
    cobalt_shm_unlink(name);

    cobalt_shm_t *shm = NULL;
    int           rc  = cobalt_shm_create(name, 1024, &shm);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(shm != NULL);
    TEST_ASSERT(cobalt_shm_get_size(shm) == 1024);

    cobalt_shm_t *shm_open = NULL;
    rc                     = cobalt_shm_open(name, &shm_open);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(shm_open != NULL);

    cobalt_shm_close(shm_open);
    cobalt_shm_close(shm);

    rc = cobalt_shm_unlink(name);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    printf("  Name formatting test passed\n");
}

static void test_notifier_anonymous(void)
{
    printf("Testing cobalt_notifier anonymous...\n");

    cobalt_notifier_t *notif = NULL;
    int                rc    = cobalt_notifier_create(NULL, &notif);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(notif != NULL);

    int fd = cobalt_notifier_get_fd(notif);
#ifndef _WIN32
    TEST_ASSERT(fd >= 0);
#else
    TEST_ASSERT(fd == -1);
#endif

    /* Non-blocking wait returns timeout when no event pending */
    rc = cobalt_notifier_wait(notif, 0);
    TEST_ASSERT(rc == COBALT_ERROR_TIMEOUT);

    /* Signal/notify */
    rc = cobalt_notifier_notify(notif);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Wait with timeout should succeed and drain the event */
    rc = cobalt_notifier_wait(notif, 1000);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Subsequent non-blocking wait returns timeout because it was drained */
    rc = cobalt_notifier_wait(notif, 0);
    TEST_ASSERT(rc == COBALT_ERROR_TIMEOUT);

    /* Multiple notifications coalescing check */
    rc = cobalt_notifier_notify(notif);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    rc = cobalt_notifier_notify(notif);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Wait should succeed and drain */
    rc = cobalt_notifier_wait(notif, 1000);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    rc = cobalt_notifier_wait(notif, 0);
    TEST_ASSERT(rc == COBALT_ERROR_TIMEOUT);

    /* Test infinite timeout wait after notification */
    rc = cobalt_notifier_notify(notif);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    rc = cobalt_notifier_wait(notif, -1);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_notifier_destroy(notif);
    printf("  Anonymous notifier test passed\n");
}

static void test_notifier_named(void)
{
    printf("Testing cobalt_notifier named...\n");

    const char *name = "/cobalt_test_notifier_named";

    /* Create named notifier A */
    cobalt_notifier_t *notif_a = NULL;
    int                rc      = cobalt_notifier_create(name, &notif_a);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(notif_a != NULL);

    /* Attempt to create again with same name should fail with ALREADY_EXISTS */
    cobalt_notifier_t *notif_dup = NULL;
    int                dup_rc    = cobalt_notifier_create(name, &notif_dup);
    TEST_ASSERT(dup_rc == COBALT_ERROR_ALREADY_EXISTS);
    TEST_ASSERT(notif_dup == NULL);

    /* Open existing named notifier B */
    cobalt_notifier_t *notif_b = NULL;
    rc                         = cobalt_notifier_open(name, &notif_b);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(notif_b != NULL);

    /* Non-blocking wait in B before notification */
    rc = cobalt_notifier_wait(notif_b, 0);
    TEST_ASSERT(rc == COBALT_ERROR_TIMEOUT);

    /* Notify from A */
    rc = cobalt_notifier_notify(notif_a);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Wait in B returns success */
    rc = cobalt_notifier_wait(notif_b, 1000);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Subsequent wait in B returns timeout (drained) */
    rc = cobalt_notifier_wait(notif_b, 0);
    TEST_ASSERT(rc == COBALT_ERROR_TIMEOUT);

    /* Reverse notification: notify from B, wait in A */
    rc = cobalt_notifier_notify(notif_b);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    rc = cobalt_notifier_wait(notif_a, 1000);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    rc = cobalt_notifier_wait(notif_a, 0);
    TEST_ASSERT(rc == COBALT_ERROR_TIMEOUT);

    /* Close both */
    cobalt_notifier_destroy(notif_b);
    cobalt_notifier_destroy(notif_a);

    /* After creator destroys it, open should return NOT_FOUND */
    cobalt_notifier_t *notif_c = NULL;
    rc                         = cobalt_notifier_open(name, &notif_c);
    TEST_ASSERT(rc == COBALT_ERROR_NOT_FOUND);
    TEST_ASSERT(notif_c == NULL);

    /* Test unslashed name normalization */
    cobalt_notifier_t *notif_noslash = NULL;
    rc = cobalt_notifier_create("cobalt_test_notif_noslash", &notif_noslash);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(notif_noslash != NULL);
    cobalt_notifier_destroy(notif_noslash);

    printf("  Named notifier test passed\n");
}

static void test_notifier_invalid_args(void)
{
    printf("Testing cobalt_notifier invalid arguments...\n");

    cobalt_notifier_t *notif = NULL;

    /* create: NULL out_notif */
    TEST_ASSERT(cobalt_notifier_create(NULL, NULL) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_notifier_create("test_notif", NULL) == COBALT_ERROR_INVALID_ARGUMENT);

    /* create: empty name or root slash */
    TEST_ASSERT(cobalt_notifier_create("", &notif) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_notifier_create("/", &notif) == COBALT_ERROR_INVALID_ARGUMENT);

    /* open: NULL name */
    TEST_ASSERT(cobalt_notifier_open(NULL, &notif) == COBALT_ERROR_INVALID_ARGUMENT);

    /* open: NULL out_notif */
    TEST_ASSERT(cobalt_notifier_open("test_notif", NULL) == COBALT_ERROR_INVALID_ARGUMENT);

    /* open: non-existent name */
    TEST_ASSERT(cobalt_notifier_open("/cobalt_nonexistent_notif_9999", &notif) ==
                COBALT_ERROR_NOT_FOUND);

    /* notify: NULL handle */
    TEST_ASSERT(cobalt_notifier_notify(NULL) == COBALT_ERROR_INVALID_ARGUMENT);

    /* wait: NULL handle */
    TEST_ASSERT(cobalt_notifier_wait(NULL, 0) == COBALT_ERROR_INVALID_ARGUMENT);

    /* wait: invalid negative timeout (< -1) */
    int rc = cobalt_notifier_create(NULL, &notif);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(notif != NULL);

    TEST_ASSERT(cobalt_notifier_wait(notif, -2) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_notifier_wait(notif, -100) == COBALT_ERROR_INVALID_ARGUMENT);

    /* get_fd: NULL handle returns -1 */
    TEST_ASSERT(cobalt_notifier_get_fd(NULL) == -1);

    /* destroy: NULL handle is safe no-op */
    cobalt_notifier_destroy(NULL);

    cobalt_notifier_destroy(notif);

    printf("  Invalid arguments test passed\n");
}

static void test_spsc_reserve_commit_peek_consume(void)
{
    printf("Testing cobalt_shm_spsc reserve, commit, peek, and consume...\n");

    const char *name = "/cobalt_test_spsc_basic";
    cobalt_shm_unlink(name);

    size_t capacity   = 65536;
    size_t total_size = sizeof(cobalt_shm_header_t) + capacity;

    cobalt_shm_t *shm_prod = NULL;
    int           rc       = cobalt_shm_create(name, total_size, &shm_prod);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(shm_prod != NULL);

    cobalt_shm_spsc_t *spsc_prod = NULL;
    rc = cobalt_shm_spsc_init(cobalt_shm_get_addr(shm_prod), total_size, capacity, &spsc_prod);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(spsc_prod != NULL);

    cobalt_shm_header_t *hdr = cobalt_shm_spsc_get_header(spsc_prod);
    TEST_ASSERT(hdr != NULL);
    TEST_ASSERT(hdr->magic == COBALT_SHM_MAGIC);
    TEST_ASSERT(hdr->version == COBALT_SHM_VERSION);
    TEST_ASSERT(hdr->mode == COBALT_SHM_MODE_SPSC);
    TEST_ASSERT(hdr->capacity == capacity);

    cobalt_shm_t *shm_cons = NULL;
    rc                     = cobalt_shm_open(name, &shm_cons);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(shm_cons != NULL);

    cobalt_shm_spsc_t *spsc_cons = NULL;
    rc = cobalt_shm_spsc_attach(cobalt_shm_get_addr(shm_cons), total_size, &spsc_cons);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(spsc_cons != NULL);

    /* Write 100 messages of varying sizes */
    for (int i = 0; i < 100; i++) {
        size_t len  = (size_t)((i * 37) % 256 + 1);
        void  *wptr = NULL;
        rc          = cobalt_shm_spsc_reserve(spsc_prod, len, &wptr);
        TEST_ASSERT(rc == COBALT_SUCCESS);
        TEST_ASSERT(wptr != NULL);

        uint8_t *bytes = (uint8_t *)wptr;
        for (size_t j = 0; j < len; j++) {
            bytes[j] = (uint8_t)((i + j) & 0xFF);
        }

        uint16_t msg_type = (uint16_t)(1000 + i);
        rc                = cobalt_shm_spsc_commit(spsc_prod, msg_type, len);
        TEST_ASSERT(rc == COBALT_SUCCESS);
    }

    /* Read and verify all 100 messages */
    for (int i = 0; i < 100; i++) {
        size_t      expected_len = (size_t)((i * 37) % 256 + 1);
        uint16_t    type         = 0;
        const void *rptr         = NULL;
        size_t      rlen         = 0;

        rc = cobalt_shm_spsc_peek(spsc_cons, &type, &rptr, &rlen);
        TEST_ASSERT(rc == COBALT_SUCCESS);
        TEST_ASSERT(type == (uint16_t)(1000 + i));
        TEST_ASSERT(rlen == expected_len);
        TEST_ASSERT(rptr != NULL);

        const uint8_t *bytes = (const uint8_t *)rptr;
        for (size_t j = 0; j < rlen; j++) {
            TEST_ASSERT(bytes[j] == (uint8_t)((i + j) & 0xFF));
        }

        rc = cobalt_shm_spsc_consume(spsc_cons);
        TEST_ASSERT(rc == COBALT_SUCCESS);
    }

    /* Ring buffer should now be empty */
    uint16_t    type = 0;
    const void *rptr = NULL;
    size_t      rlen = 0;
    rc               = cobalt_shm_spsc_peek(spsc_cons, &type, &rptr, &rlen);
    TEST_ASSERT(rc == COBALT_ERROR_EMPTY_CONTAINER);

    cobalt_shm_spsc_detach(spsc_cons);
    cobalt_shm_spsc_detach(spsc_prod);
    cobalt_shm_close(shm_cons);
    cobalt_shm_close(shm_prod);
    cobalt_shm_unlink(name);

    printf("  SPSC reserve, commit, peek, consume test passed\n");
}

static void test_spsc_wraparound(void)
{
    printf("Testing cobalt_shm_spsc wraparound...\n");

    const char *name = "/cobalt_test_spsc_wrap";
    cobalt_shm_unlink(name);

    size_t capacity   = 1024;
    size_t total_size = sizeof(cobalt_shm_header_t) + capacity;

    cobalt_shm_t *shm = NULL;
    int           rc  = cobalt_shm_create(name, total_size, &shm);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_shm_spsc_t *spsc_prod = NULL;
    rc = cobalt_shm_spsc_init(cobalt_shm_get_addr(shm), total_size, capacity, &spsc_prod);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_shm_spsc_t *spsc_cons = NULL;
    rc = cobalt_shm_spsc_attach(cobalt_shm_get_addr(shm), total_size, &spsc_cons);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Write and consume 50 messages of 120 bytes each.
     * Each frame is ALIGN_UP(8 + 120, 8) = 128 bytes.
     * In a 1024-byte buffer, 128 * 50 = 6400 bytes will wrap around 6+ times.
     */
    for (int i = 0; i < 50; i++) {
        size_t len  = 120;
        void  *wptr = NULL;
        rc          = cobalt_shm_spsc_reserve(spsc_prod, len, &wptr);
        TEST_ASSERT(rc == COBALT_SUCCESS);
        TEST_ASSERT(wptr != NULL);

        uint8_t *bytes = (uint8_t *)wptr;
        for (size_t j = 0; j < len; j++) {
            bytes[j] = (uint8_t)((i * 7 + j) & 0xFF);
        }

        rc = cobalt_shm_spsc_commit(spsc_prod, (uint16_t)(i + 1), len);
        TEST_ASSERT(rc == COBALT_SUCCESS);

        /* Consume immediately to keep space in ring */
        uint16_t    type = 0;
        const void *rptr = NULL;
        size_t      rlen = 0;
        rc               = cobalt_shm_spsc_peek(spsc_cons, &type, &rptr, &rlen);
        TEST_ASSERT(rc == COBALT_SUCCESS);
        TEST_ASSERT(type == (uint16_t)(i + 1));
        TEST_ASSERT(rlen == len);

        const uint8_t *rbytes = (const uint8_t *)rptr;
        for (size_t j = 0; j < rlen; j++) {
            TEST_ASSERT(rbytes[j] == (uint8_t)((i * 7 + j) & 0xFF));
        }

        rc = cobalt_shm_spsc_consume(spsc_cons);
        TEST_ASSERT(rc == COBALT_SUCCESS);
    }

    /* Verify empty at end */
    uint16_t    type = 0;
    const void *rptr = NULL;
    size_t      rlen = 0;
    rc               = cobalt_shm_spsc_peek(spsc_cons, &type, &rptr, &rlen);
    TEST_ASSERT(rc == COBALT_ERROR_EMPTY_CONTAINER);

    cobalt_shm_spsc_detach(spsc_cons);
    cobalt_shm_spsc_detach(spsc_prod);
    cobalt_shm_close(shm);
    cobalt_shm_unlink(name);

    printf("  SPSC wraparound test passed\n");
}

static void test_spsc_queue_full(void)
{
    printf("Testing cobalt_shm_spsc queue full condition...\n");

    const char *name = "/cobalt_test_spsc_full";
    cobalt_shm_unlink(name);

    size_t capacity   = 1024;
    size_t total_size = sizeof(cobalt_shm_header_t) + capacity;

    cobalt_shm_t *shm = NULL;
    int           rc  = cobalt_shm_create(name, total_size, &shm);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_shm_spsc_t *spsc = NULL;
    rc = cobalt_shm_spsc_init(cobalt_shm_get_addr(shm), total_size, capacity, &spsc);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Each message: payload 200 bytes -> frame size ALIGN_UP(8 + 200, 8) = 208 bytes.
     * In 1024 bytes, 4 messages take 832 bytes.
     * The 5th message requires 208 bytes, but remaining space is 1024 - 832 = 192 < 208.
     * So the 5th message will trigger QUEUE_FULL!
     */
    int count = 0;
    while (1) {
        void *wptr = NULL;
        rc         = cobalt_shm_spsc_reserve(spsc, 200, &wptr);
        if (rc == COBALT_ERROR_QUEUE_FULL) {
            break;
        }
        TEST_ASSERT(rc == COBALT_SUCCESS);
        rc = cobalt_shm_spsc_commit(spsc, 1, 200);
        TEST_ASSERT(rc == COBALT_SUCCESS);
        count++;
    }
    TEST_ASSERT(count == 4);

    /* Queue is full, another reserve must fail */
    void *wptr = NULL;
    rc         = cobalt_shm_spsc_reserve(spsc, 200, &wptr);
    TEST_ASSERT(rc == COBALT_ERROR_QUEUE_FULL);

    /* Consume one message */
    uint16_t    type = 0;
    const void *rptr = NULL;
    size_t      rlen = 0;
    rc               = cobalt_shm_spsc_peek(spsc, &type, &rptr, &rlen);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(rlen == 200);
    rc = cobalt_shm_spsc_consume(spsc);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Now reserve should succeed! */
    rc = cobalt_shm_spsc_reserve(spsc, 200, &wptr);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    rc = cobalt_shm_spsc_commit(spsc, 2, 200);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Drain all 4 remaining messages */
    for (int i = 0; i < 4; i++) {
        rc = cobalt_shm_spsc_peek(spsc, &type, &rptr, &rlen);
        TEST_ASSERT(rc == COBALT_SUCCESS);
        rc = cobalt_shm_spsc_consume(spsc);
        TEST_ASSERT(rc == COBALT_SUCCESS);
    }

    /* Now empty */
    rc = cobalt_shm_spsc_peek(spsc, &type, &rptr, &rlen);
    TEST_ASSERT(rc == COBALT_ERROR_EMPTY_CONTAINER);

    cobalt_shm_spsc_detach(spsc);
    cobalt_shm_close(shm);
    cobalt_shm_unlink(name);

    printf("  SPSC queue full test passed\n");
}

static void test_spsc_invalid_args(void)
{
    printf("Testing cobalt_shm_spsc invalid arguments and edge cases...\n");

    cobalt_shm_spsc_t   *spsc = NULL;
    _Alignas(64) uint8_t buffer[2048];

    /* init: NULL address */
    TEST_ASSERT(cobalt_shm_spsc_init(NULL, sizeof(buffer), 1024, &spsc) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: unaligned address */
    TEST_ASSERT(cobalt_shm_spsc_init(buffer + 1, sizeof(buffer) - 1, 1024, &spsc) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: NULL out_spsc */
    TEST_ASSERT(cobalt_shm_spsc_init(buffer, sizeof(buffer), 1024, NULL) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: non-power-of-2 capacity */
    TEST_ASSERT(cobalt_shm_spsc_init(buffer, sizeof(buffer), 1000, &spsc) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: 0 capacity */
    TEST_ASSERT(cobalt_shm_spsc_init(buffer, sizeof(buffer), 0, &spsc) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: total_size too small */
    TEST_ASSERT(cobalt_shm_spsc_init(buffer, 100, 1024, &spsc) == COBALT_ERROR_INVALID_ARGUMENT);

    /* Valid init for further tests */
    int rc = cobalt_shm_spsc_init(buffer, sizeof(buffer), 1024, &spsc);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(spsc != NULL);

    /* attach: unaligned address */
    cobalt_shm_spsc_t *spsc_att = NULL;
    TEST_ASSERT(cobalt_shm_spsc_attach(buffer + 1, sizeof(buffer) - 1, &spsc_att) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* attach: NULL address or out_spsc */
    TEST_ASSERT(cobalt_shm_spsc_attach(NULL, sizeof(buffer), &spsc_att) ==
                COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_shm_spsc_attach(buffer, sizeof(buffer), NULL) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* attach: undersized total_size */
    TEST_ASSERT(cobalt_shm_spsc_attach(buffer, 100, &spsc_att) == COBALT_ERROR_INVALID_ARGUMENT);

    /* reserve: NULL spsc or NULL out_ptr */
    void *ptr = NULL;
    TEST_ASSERT(cobalt_shm_spsc_reserve(NULL, 10, &ptr) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_shm_spsc_reserve(spsc, 10, NULL) == COBALT_ERROR_INVALID_ARGUMENT);

    /* reserve: message larger than capacity allows */
    TEST_ASSERT(cobalt_shm_spsc_reserve(spsc, 2000, &ptr) == COBALT_ERROR_INVALID_ARGUMENT);

    /* commit without reserve */
    TEST_ASSERT(cobalt_shm_spsc_commit(spsc, 1, 10) == COBALT_ERROR_INVALID_ARGUMENT);

    /* cancel without reserve */
    TEST_ASSERT(cobalt_shm_spsc_cancel(spsc) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_shm_spsc_cancel(NULL) == COBALT_ERROR_INVALID_ARGUMENT);

    /* reserve and then cancel */
    rc = cobalt_shm_spsc_reserve(spsc, 64, &ptr);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(cobalt_shm_spsc_cancel(spsc) == COBALT_SUCCESS);
    TEST_ASSERT(cobalt_shm_spsc_cancel(spsc) == COBALT_ERROR_INVALID_ARGUMENT);

    /* consume without peek */
    TEST_ASSERT(cobalt_shm_spsc_consume(spsc) == COBALT_ERROR_INVALID_ARGUMENT);

    /* double reserve */
    rc = cobalt_shm_spsc_reserve(spsc, 64, &ptr);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(cobalt_shm_spsc_reserve(spsc, 64, &ptr) == COBALT_ERROR_INVALID_ARGUMENT);

    /* commit with len > reserved_len */
    TEST_ASSERT(cobalt_shm_spsc_commit(spsc, 1, 128) == COBALT_ERROR_INVALID_ARGUMENT);

    /* commit valid */
    TEST_ASSERT(cobalt_shm_spsc_commit(spsc, 1, 64) == COBALT_SUCCESS);

    /* peek: NULL spsc or NULL out_ptr */
    uint16_t    type = 0;
    size_t      len  = 0;
    const void *rptr = NULL;
    TEST_ASSERT(cobalt_shm_spsc_peek(NULL, &type, &rptr, &len) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_shm_spsc_peek(spsc, &type, NULL, &len) == COBALT_ERROR_INVALID_ARGUMENT);

    /* consume after peek */
    TEST_ASSERT(cobalt_shm_spsc_peek(spsc, &type, &rptr, &len) == COBALT_SUCCESS);
    TEST_ASSERT(cobalt_shm_spsc_consume(spsc) == COBALT_SUCCESS);

    /* double consume */
    TEST_ASSERT(cobalt_shm_spsc_consume(spsc) == COBALT_ERROR_INVALID_ARGUMENT);

    /* get_header: NULL returns NULL */
    TEST_ASSERT(cobalt_shm_spsc_get_header(NULL) == NULL);

    /* detach: NULL is safe no-op */
    cobalt_shm_spsc_detach(NULL);

    cobalt_shm_spsc_detach(spsc);

    printf("  SPSC invalid arguments test passed\n");
}

static void test_mpmc_basic_push_pop(void)
{
    printf("Testing cobalt_shm_mpmc basic push and pop...\n");

    const char *name = "/cobalt_test_mpmc_basic";
    cobalt_shm_unlink(name);

    size_t capacity    = 128;
    size_t slot_size   = 64;
    size_t slot_stride = 128; /* ALIGN_UP(16 + 64, 64) */
    size_t total_size  = sizeof(cobalt_shm_header_t) + capacity * slot_stride;

    cobalt_shm_t *shm_prod = NULL;
    int           rc       = cobalt_shm_create(name, total_size, &shm_prod);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(shm_prod != NULL);

    cobalt_shm_mpmc_t *mpmc_prod = NULL;
    rc                           = cobalt_shm_mpmc_init(
        cobalt_shm_get_addr(shm_prod), total_size, capacity, slot_size, &mpmc_prod);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(mpmc_prod != NULL);
    TEST_ASSERT(cobalt_shm_mpmc_capacity(mpmc_prod) == capacity);
    TEST_ASSERT(cobalt_shm_mpmc_slot_size(mpmc_prod) == slot_size);

    cobalt_shm_header_t *hdr = cobalt_shm_mpmc_get_header(mpmc_prod);
    TEST_ASSERT(hdr != NULL);
    TEST_ASSERT(hdr->magic == COBALT_SHM_MAGIC);
    TEST_ASSERT(hdr->version == COBALT_SHM_VERSION);
    TEST_ASSERT(hdr->mode == COBALT_SHM_MODE_MPMC);
    TEST_ASSERT(hdr->capacity == capacity);
    TEST_ASSERT(hdr->slot_size == (uint32_t)slot_size);

    cobalt_shm_t *shm_cons = NULL;
    rc                     = cobalt_shm_open(name, &shm_cons);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(shm_cons != NULL);

    cobalt_shm_mpmc_t *mpmc_cons = NULL;
    rc = cobalt_shm_mpmc_attach(cobalt_shm_get_addr(shm_cons), total_size, &mpmc_cons);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(mpmc_cons != NULL);
    TEST_ASSERT(cobalt_shm_mpmc_capacity(mpmc_cons) == capacity);
    TEST_ASSERT(cobalt_shm_mpmc_slot_size(mpmc_cons) == slot_size);

    /* Write 100 messages of varying sizes */
    for (int i = 0; i < 100; i++) {
        uint8_t payload[64];
        size_t  len = (size_t)((i * 17) % 64 + 1);
        for (size_t j = 0; j < len; j++) {
            payload[j] = (uint8_t)((i * 3 + j) & 0xFF);
        }
        rc = cobalt_shm_mpmc_push(mpmc_prod, payload, len);
        TEST_ASSERT(rc == COBALT_SUCCESS);
    }

    /* Read and verify all 100 messages */
    for (int i = 0; i < 100; i++) {
        uint8_t buf[64];
        size_t  out_len      = 0;
        size_t  expected_len = (size_t)((i * 17) % 64 + 1);

        rc = cobalt_shm_mpmc_pop(mpmc_cons, buf, sizeof(buf), &out_len);
        TEST_ASSERT(rc == COBALT_SUCCESS);
        TEST_ASSERT(out_len == expected_len);

        for (size_t j = 0; j < out_len; j++) {
            TEST_ASSERT(buf[j] == (uint8_t)((i * 3 + j) & 0xFF));
        }
    }

    /* Verify ring buffer is empty */
    uint8_t buf[64];
    size_t  out_len = 0;
    rc              = cobalt_shm_mpmc_pop(mpmc_cons, buf, sizeof(buf), &out_len);
    TEST_ASSERT(rc == COBALT_ERROR_EMPTY_CONTAINER);

    cobalt_shm_mpmc_detach(mpmc_cons);
    cobalt_shm_mpmc_detach(mpmc_prod);
    cobalt_shm_close(shm_cons);
    cobalt_shm_close(shm_prod);
    cobalt_shm_unlink(name);

    printf("  MPMC basic push and pop test passed\n");
}

static void test_mpmc_queue_full_and_empty(void)
{
    printf("Testing cobalt_shm_mpmc queue full and empty conditions across revolutions...\n");

    const char *name = "/cobalt_test_mpmc_full_empty";
    cobalt_shm_unlink(name);

    size_t capacity    = 16;
    size_t slot_size   = 32;
    size_t slot_stride = 64; /* ALIGN_UP(16 + 32, 64) */
    size_t total_size  = sizeof(cobalt_shm_header_t) + capacity * slot_stride;

    cobalt_shm_t *shm = NULL;
    int           rc  = cobalt_shm_create(name, total_size, &shm);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_shm_mpmc_t *mpmc = NULL;
    rc = cobalt_shm_mpmc_init(cobalt_shm_get_addr(shm), total_size, capacity, slot_size, &mpmc);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Test multiple revolutions */
    for (int rev = 0; rev < 5; rev++) {
        /* Empty check before push */
        uint32_t val     = 0;
        size_t   out_len = 0;
        rc               = cobalt_shm_mpmc_pop(mpmc, &val, sizeof(val), &out_len);
        TEST_ASSERT(rc == COBALT_ERROR_EMPTY_CONTAINER);

        /* Fill exactly 16 slots */
        for (int i = 0; i < 16; i++) {
            uint32_t item = (uint32_t)(rev * 1000 + i);
            rc            = cobalt_shm_mpmc_push(mpmc, &item, sizeof(item));
            TEST_ASSERT(rc == COBALT_SUCCESS);
        }

        /* 17th push should return QUEUE_FULL */
        uint32_t extra = 99999;
        rc             = cobalt_shm_mpmc_push(mpmc, &extra, sizeof(extra));
        TEST_ASSERT(rc == COBALT_ERROR_QUEUE_FULL);

        /* Drain all 16 slots */
        for (int i = 0; i < 16; i++) {
            val     = 0;
            out_len = 0;
            rc      = cobalt_shm_mpmc_pop(mpmc, &val, sizeof(val), &out_len);
            TEST_ASSERT(rc == COBALT_SUCCESS);
            TEST_ASSERT(out_len == sizeof(uint32_t));
            TEST_ASSERT(val == (uint32_t)(rev * 1000 + i));
        }

        /* 17th pop should return EMPTY_CONTAINER */
        rc = cobalt_shm_mpmc_pop(mpmc, &val, sizeof(val), &out_len);
        TEST_ASSERT(rc == COBALT_ERROR_EMPTY_CONTAINER);
    }

    cobalt_shm_mpmc_detach(mpmc);
    cobalt_shm_close(shm);
    cobalt_shm_unlink(name);

    printf("  MPMC queue full and empty test passed\n");
}

#define MPMC_NUM_PRODUCERS 4
#define MPMC_NUM_CONSUMERS 4
#define MPMC_ITEMS_PER_PRODUCER 12500
#define MPMC_TOTAL_ITEMS (MPMC_NUM_PRODUCERS * MPMC_ITEMS_PER_PRODUCER)
#define MPMC_ITEMS_PER_CONSUMER (MPMC_TOTAL_ITEMS / MPMC_NUM_CONSUMERS)

typedef struct {
    cobalt_shm_mpmc_t *mpmc;
    int                thread_id;
} mpmc_worker_arg_t;

static uint64_t g_mpmc_consumer_sums[MPMC_NUM_CONSUMERS];
static size_t   g_mpmc_consumer_counts[MPMC_NUM_CONSUMERS];

static void *mpmc_producer_worker(void *arg)
{
    mpmc_worker_arg_t *w = (mpmc_worker_arg_t *)arg;
    for (int i = 0; i < MPMC_ITEMS_PER_PRODUCER; i++) {
        uint64_t val = (uint64_t)(w->thread_id * MPMC_ITEMS_PER_PRODUCER + i + 1);
        while (cobalt_shm_mpmc_push(w->mpmc, &val, sizeof(val)) == COBALT_ERROR_QUEUE_FULL) {
            cobalt_thread_yield();
        }
    }
    return NULL;
}

static void *mpmc_consumer_worker(void *arg)
{
    mpmc_worker_arg_t *w            = (mpmc_worker_arg_t *)arg;
    uint64_t           sum          = 0;
    size_t             items_popped = 0;

    while (items_popped < MPMC_ITEMS_PER_CONSUMER) {
        uint64_t val     = 0;
        size_t   out_len = 0;
        int      rc      = cobalt_shm_mpmc_pop(w->mpmc, &val, sizeof(val), &out_len);
        if (rc == COBALT_SUCCESS) {
            TEST_ASSERT(out_len == sizeof(val));
            sum += val;
            items_popped++;
        } else if (rc == COBALT_ERROR_EMPTY_CONTAINER) {
            cobalt_thread_yield();
        }
    }
    g_mpmc_consumer_sums[w->thread_id]   = sum;
    g_mpmc_consumer_counts[w->thread_id] = items_popped;
    return NULL;
}

static void test_mpmc_multithreaded_contention(void)
{
    printf("Testing cobalt_shm_mpmc multithreaded contention (4P + 4C, 50,000 items)...\n");

    const char *name = "/cobalt_test_mpmc_mt";
    cobalt_shm_unlink(name);

    size_t capacity    = 1024;
    size_t slot_size   = sizeof(uint64_t);
    size_t slot_stride = 64; /* ALIGN_UP(16 + 8, 64) */
    size_t total_size  = sizeof(cobalt_shm_header_t) + capacity * slot_stride;

    cobalt_shm_t *shm = NULL;
    int           rc  = cobalt_shm_create(name, total_size, &shm);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_shm_mpmc_t *mpmc = NULL;
    rc = cobalt_shm_mpmc_init(cobalt_shm_get_addr(shm), total_size, capacity, slot_size, &mpmc);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_thread_t   producers[MPMC_NUM_PRODUCERS];
    cobalt_thread_t   consumers[MPMC_NUM_CONSUMERS];
    mpmc_worker_arg_t prod_args[MPMC_NUM_PRODUCERS];
    mpmc_worker_arg_t cons_args[MPMC_NUM_CONSUMERS];

    for (int i = 0; i < MPMC_NUM_CONSUMERS; i++) {
        cons_args[i].mpmc         = mpmc;
        cons_args[i].thread_id    = i;
        g_mpmc_consumer_sums[i]   = 0;
        g_mpmc_consumer_counts[i] = 0;
        TEST_ASSERT(cobalt_thread_create(mpmc_consumer_worker, &cons_args[i], &consumers[i]) == 0);
    }

    for (int i = 0; i < MPMC_NUM_PRODUCERS; i++) {
        prod_args[i].mpmc      = mpmc;
        prod_args[i].thread_id = i;
        TEST_ASSERT(cobalt_thread_create(mpmc_producer_worker, &prod_args[i], &producers[i]) == 0);
    }

    for (int i = 0; i < MPMC_NUM_PRODUCERS; i++) {
        cobalt_thread_join(producers[i]);
    }
    for (int i = 0; i < MPMC_NUM_CONSUMERS; i++) {
        cobalt_thread_join(consumers[i]);
    }

    size_t   total_items_consumed = 0;
    uint64_t total_sum            = 0;
    for (int i = 0; i < MPMC_NUM_CONSUMERS; i++) {
        total_items_consumed += g_mpmc_consumer_counts[i];
        total_sum += g_mpmc_consumer_sums[i];
    }

    TEST_ASSERT(total_items_consumed == (size_t)MPMC_TOTAL_ITEMS);

    uint64_t expected_sum = 0;
    for (int p = 0; p < MPMC_NUM_PRODUCERS; p++) {
        for (int i = 0; i < MPMC_ITEMS_PER_PRODUCER; i++) {
            expected_sum += (uint64_t)(p * MPMC_ITEMS_PER_PRODUCER + i + 1);
        }
    }
    TEST_ASSERT(total_sum == expected_sum);

    /* Verify ring is now empty */
    uint64_t dummy   = 0;
    size_t   out_len = 0;
    rc               = cobalt_shm_mpmc_pop(mpmc, &dummy, sizeof(dummy), &out_len);
    TEST_ASSERT(rc == COBALT_ERROR_EMPTY_CONTAINER);

    cobalt_shm_mpmc_detach(mpmc);
    cobalt_shm_close(shm);
    cobalt_shm_unlink(name);

    printf("  MPMC multithreaded contention test passed\n");
}

static void test_mpmc_invalid_args(void)
{
    printf("Testing cobalt_shm_mpmc invalid arguments and edge cases...\n");

    cobalt_shm_mpmc_t   *mpmc = NULL;
    _Alignas(64) uint8_t buffer[4096];

    /* init: NULL address */
    TEST_ASSERT(cobalt_shm_mpmc_init(NULL, sizeof(buffer), 16, 32, &mpmc) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: unaligned address */
    TEST_ASSERT(cobalt_shm_mpmc_init(buffer + 1, sizeof(buffer) - 1, 16, 32, &mpmc) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: NULL out_mpmc */
    TEST_ASSERT(cobalt_shm_mpmc_init(buffer, sizeof(buffer), 16, 32, NULL) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: capacity 0 */
    TEST_ASSERT(cobalt_shm_mpmc_init(buffer, sizeof(buffer), 0, 32, &mpmc) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: capacity 1 */
    TEST_ASSERT(cobalt_shm_mpmc_init(buffer, sizeof(buffer), 1, 32, &mpmc) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: non-power-of-two capacity */
    TEST_ASSERT(cobalt_shm_mpmc_init(buffer, sizeof(buffer), 10, 32, &mpmc) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: slot_size 0 */
    TEST_ASSERT(cobalt_shm_mpmc_init(buffer, sizeof(buffer), 16, 0, &mpmc) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* init: total_size too small */
    TEST_ASSERT(cobalt_shm_mpmc_init(buffer, 100, 16, 32, &mpmc) == COBALT_ERROR_INVALID_ARGUMENT);

    /* Valid init */
    int rc = cobalt_shm_mpmc_init(buffer, sizeof(buffer), 16, 32, &mpmc);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(mpmc != NULL);

    /* attach: unaligned address */
    cobalt_shm_mpmc_t *mpmc_att = NULL;
    TEST_ASSERT(cobalt_shm_mpmc_attach(buffer + 1, sizeof(buffer) - 1, &mpmc_att) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* attach: NULL address or NULL out_mpmc */
    TEST_ASSERT(cobalt_shm_mpmc_attach(NULL, sizeof(buffer), &mpmc_att) ==
                COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_shm_mpmc_attach(buffer, sizeof(buffer), NULL) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* attach: undersized total_size */
    TEST_ASSERT(cobalt_shm_mpmc_attach(buffer, 100, &mpmc_att) == COBALT_ERROR_INVALID_ARGUMENT);

    /* attach: corrupted header mode */
    cobalt_shm_header_t *hdr = (cobalt_shm_header_t *)buffer;
    hdr->mode                = COBALT_SHM_MODE_SPSC;
    TEST_ASSERT(cobalt_shm_mpmc_attach(buffer, sizeof(buffer), &mpmc_att) ==
                COBALT_ERROR_INVALID_ARGUMENT);
    hdr->mode = COBALT_SHM_MODE_MPMC;

    /* push: NULL handle or NULL data */
    uint32_t data = 42;
    TEST_ASSERT(cobalt_shm_mpmc_push(NULL, &data, sizeof(data)) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_shm_mpmc_push(mpmc, NULL, sizeof(data)) == COBALT_ERROR_INVALID_ARGUMENT);

    /* push: payload len > slot_size (32) */
    uint8_t large_data[64];
    TEST_ASSERT(cobalt_shm_mpmc_push(mpmc, large_data, 64) == COBALT_ERROR_INVALID_ARGUMENT);

    /* pop: NULL handle */
    uint32_t out_buf = 0;
    size_t   out_len = 0;
    TEST_ASSERT(cobalt_shm_mpmc_pop(NULL, &out_buf, sizeof(out_buf), &out_len) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* pop: buf NULL but buf_size > 0 */
    TEST_ASSERT(cobalt_shm_mpmc_pop(mpmc, NULL, sizeof(out_buf), &out_len) ==
                COBALT_ERROR_INVALID_ARGUMENT);

    /* pop: NULL out_len with valid buf is allowed */
    rc = cobalt_shm_mpmc_push(mpmc, &data, sizeof(data));
    TEST_ASSERT(rc == COBALT_SUCCESS);
    rc = cobalt_shm_mpmc_pop(mpmc, &out_buf, sizeof(out_buf), NULL);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(out_buf == 42);

    /* getters on NULL handle */
    TEST_ASSERT(cobalt_shm_mpmc_capacity(NULL) == 0);
    TEST_ASSERT(cobalt_shm_mpmc_slot_size(NULL) == 0);
    TEST_ASSERT(cobalt_shm_mpmc_get_header(NULL) == NULL);

    /* detach: NULL is safe no-op */
    cobalt_shm_mpmc_detach(NULL);

    cobalt_shm_mpmc_detach(mpmc);

    printf("  MPMC invalid arguments test passed\n");
}

void test_shm(void)
{
    printf("Testing shared memory primitive (cobalt_shm_t)...\n");
    test_shm_create_and_close();
    test_shm_open_existing();
    test_shm_invalid_args();
    test_shm_name_formatting();
    printf("  Shared memory tests completed successfully!\n\n");

    printf("Testing notification primitive (cobalt_notifier_t)...\n");
    test_notifier_anonymous();
    test_notifier_named();
    test_notifier_invalid_args();
    printf("  Notification tests completed successfully!\n\n");

    printf("Testing lock-free SPSC shared memory ring (cobalt_shm_spsc_t)...\n");
    test_spsc_reserve_commit_peek_consume();
    test_spsc_wraparound();
    test_spsc_queue_full();
    test_spsc_invalid_args();
    printf("  SPSC shared memory ring tests completed successfully!\n\n");

    printf("Testing lock-free MPMC shared memory ring (cobalt_shm_mpmc_t)...\n");
    test_mpmc_basic_push_pop();
    test_mpmc_queue_full_and_empty();
    test_mpmc_multithreaded_contention();
    test_mpmc_invalid_args();
    printf("  MPMC shared memory ring tests completed successfully!\n");
}
