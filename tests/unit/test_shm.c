/**
 * @file test_shm.c
 * @brief Unit tests for platform shared memory primitive (cobalt_shm_t).
 */

#include "cobalt/container/shm_ring.h"
#include "cobalt/module/eventloop.h"
#include "cobalt/module/shm_channel.h"
#include "cobalt/platform/notifier.h"
#include "cobalt/platform/shm.h"
#include "cobalt/platform/thread.h"
#include "cobalt/runtime/error.h"
#include "test_framework.h"

#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#endif

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
    TEST_ASSERT(cobalt_shm_open("/cobalt_test_nonexistent_xyz", &shm) == COBALT_ERROR_NOT_FOUND);

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
    TEST_ASSERT(cobalt_notifier_open("/cobalt_nonexist_notif", &notif) == COBALT_ERROR_NOT_FOUND);

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

    /* pop: truncation check when buf_size < actual message size */
    char test_msg[32] = "Truncation test message!";
    rc                = cobalt_shm_mpmc_push(mpmc, test_msg, strlen(test_msg));
    TEST_ASSERT(rc == COBALT_SUCCESS);
    char small_buf[10];
    memset(small_buf, 0, sizeof(small_buf));
    rc = cobalt_shm_mpmc_pop(mpmc, small_buf, sizeof(small_buf) - 1, &out_len);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(out_len == strlen(test_msg));
    TEST_ASSERT(strncmp(small_buf, "Truncatio", 9) == 0);

    /* getters on NULL handle */
    TEST_ASSERT(cobalt_shm_mpmc_capacity(NULL) == 0);
    TEST_ASSERT(cobalt_shm_mpmc_slot_size(NULL) == 0);
    TEST_ASSERT(cobalt_shm_mpmc_get_header(NULL) == NULL);

    /* detach: NULL is safe no-op */
    cobalt_shm_mpmc_detach(NULL);

    cobalt_shm_mpmc_detach(mpmc);

    printf("  MPMC invalid arguments test passed\n");
}

static void test_channel_spsc_lifecycle_and_send_recv(void)
{
    printf("Testing cobalt_shm_channel SPSC lifecycle and send/recv...\n");

    const char *name = "/cobalt_test_chan_spsc";
    cobalt_shm_unlink(name);

    cobalt_shm_config_t cfg = {
        .name              = name,
        .mode              = COBALT_SHM_MODE_SPSC,
        .wait_policy       = COBALT_SHM_WAIT_BLOCKING,
        .capacity          = 4096,
        .slot_size         = 0,
        .create_if_missing = true,
        .spin_threshold    = 1000,
    };

    cobalt_shm_channel_t *server = cobalt_shm_channel_create(&cfg);
    TEST_ASSERT(server != NULL);
    TEST_ASSERT(cobalt_shm_channel_get_mode(server) == COBALT_SHM_MODE_SPSC);
    TEST_ASSERT(cobalt_shm_channel_get_wait_policy(server) == COBALT_SHM_WAIT_BLOCKING);
    TEST_ASSERT(cobalt_shm_channel_get_read_fd(server) >= 0);

    cobalt_shm_channel_t *client = cobalt_shm_channel_open(name, COBALT_SHM_WAIT_BLOCKING);
    TEST_ASSERT(client != NULL);
    TEST_ASSERT(cobalt_shm_channel_get_mode(client) == COBALT_SHM_MODE_SPSC);
    TEST_ASSERT(cobalt_shm_channel_get_wait_policy(client) == COBALT_SHM_WAIT_BLOCKING);

    /* Send 50 messages from server to client */
    for (int i = 0; i < 50; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "spsc-msg-%03d", i);
        int rc = cobalt_shm_channel_send(server, (uint16_t)(100 + i), msg, strlen(msg) + 1, 100);
        TEST_ASSERT(rc == COBALT_SUCCESS);
    }

    /* Recv 50 messages on client */
    for (int i = 0; i < 50; i++) {
        char     buf[32];
        uint16_t type    = 0;
        size_t   out_len = 0;
        int      rc      = cobalt_shm_channel_recv(client, &type, buf, sizeof(buf), &out_len, 100);
        TEST_ASSERT(rc == COBALT_SUCCESS);
        TEST_ASSERT(type == (uint16_t)(100 + i));
        char expected[32];
        snprintf(expected, sizeof(expected), "spsc-msg-%03d", i);
        TEST_ASSERT(out_len == strlen(expected) + 1);
        TEST_ASSERT(strcmp(buf, expected) == 0);
    }

    /* Test zero-copy reserve / commit / peek / consume */
    void *wptr = NULL;
    int   rc   = cobalt_shm_channel_reserve(server, 20, &wptr);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(wptr != NULL);
    strcpy((char *)wptr, "zero-copy-test");
    rc = cobalt_shm_channel_commit(server, 777, strlen("zero-copy-test") + 1);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    uint16_t    peek_type = 0;
    const void *rptr      = NULL;
    size_t      peek_len  = 0;
    rc                    = cobalt_shm_channel_peek(client, &peek_type, &rptr, &peek_len);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(peek_type == 777);
    TEST_ASSERT(peek_len == strlen("zero-copy-test") + 1);
    TEST_ASSERT(strcmp((const char *)rptr, "zero-copy-test") == 0);

    rc = cobalt_shm_channel_consume(client);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Test cancel */
    rc = cobalt_shm_channel_reserve(server, 20, &wptr);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    rc = cobalt_shm_channel_cancel(server);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_shm_channel_destroy(client);
    cobalt_shm_channel_destroy(server);

    printf("  SPSC channel lifecycle and send/recv test passed\n");
}

static void test_channel_mpmc_lifecycle_and_send_recv(void)
{
    printf("Testing cobalt_shm_channel MPMC lifecycle and send/recv...\n");

    const char *name = "/cobalt_test_chan_mpmc";
    cobalt_shm_unlink(name);

    cobalt_shm_config_t cfg = {
        .name              = name,
        .mode              = COBALT_SHM_MODE_MPMC,
        .wait_policy       = COBALT_SHM_WAIT_BLOCKING,
        .capacity          = 64,
        .slot_size         = 64,
        .create_if_missing = true,
        .spin_threshold    = 1000,
    };

    cobalt_shm_channel_t *server = cobalt_shm_channel_create(&cfg);
    TEST_ASSERT(server != NULL);
    TEST_ASSERT(cobalt_shm_channel_get_mode(server) == COBALT_SHM_MODE_MPMC);
    TEST_ASSERT(cobalt_shm_channel_get_wait_policy(server) == COBALT_SHM_WAIT_BLOCKING);
    TEST_ASSERT(cobalt_shm_channel_get_read_fd(server) >= 0);

    cobalt_shm_channel_t *client = cobalt_shm_channel_open(name, COBALT_SHM_WAIT_BLOCKING);
    TEST_ASSERT(client != NULL);
    TEST_ASSERT(cobalt_shm_channel_get_mode(client) == COBALT_SHM_MODE_MPMC);

    /* Send 50 messages from server */
    for (int i = 0; i < 50; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "mpmc-val-%03d", i);
        int rc = cobalt_shm_channel_send(server, 0, msg, strlen(msg) + 1, 100);
        TEST_ASSERT(rc == COBALT_SUCCESS);
    }

    /* Recv 50 messages on client */
    for (int i = 0; i < 50; i++) {
        char     buf[32];
        uint16_t type    = 0;
        size_t   out_len = 0;
        int      rc      = cobalt_shm_channel_recv(client, &type, buf, sizeof(buf), &out_len, 100);
        TEST_ASSERT(rc == COBALT_SUCCESS);
        char expected[32];
        snprintf(expected, sizeof(expected), "mpmc-val-%03d", i);
        TEST_ASSERT(out_len == strlen(expected) + 1);
        TEST_ASSERT(strcmp(buf, expected) == 0);
    }

    /* SPSC zero-copy functions should return error on MPMC */
    void *dummy_ptr = NULL;
    TEST_ASSERT(cobalt_shm_channel_reserve(server, 10, &dummy_ptr) ==
                COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_shm_channel_commit(server, 1, 10) == COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_shm_channel_cancel(server) == COBALT_ERROR_INVALID_ARGUMENT);
    const void *dummy_cptr = NULL;
    size_t      dummy_len  = 0;
    uint16_t    dummy_type = 0;
    TEST_ASSERT(cobalt_shm_channel_peek(server, &dummy_type, &dummy_cptr, &dummy_len) ==
                COBALT_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(cobalt_shm_channel_consume(server) == COBALT_ERROR_INVALID_ARGUMENT);

    cobalt_shm_channel_destroy(client);
    cobalt_shm_channel_destroy(server);

    printf("  MPMC channel lifecycle and send/recv test passed\n");
}

typedef struct {
    cobalt_shm_channel_t *chan;
    int                   sleep_ms;
    int                   val;
} delayed_sender_arg_t;

static void *delayed_sender_worker(void *arg)
{
    delayed_sender_arg_t *a = (delayed_sender_arg_t *)arg;
#ifdef _WIN32
    Sleep(a->sleep_ms);
#else
    struct timespec req;
    req.tv_sec  = a->sleep_ms / 1000;
    req.tv_nsec = (long)(a->sleep_ms % 1000) * 1000000L;
    nanosleep(&req, NULL);
#endif
    cobalt_shm_channel_send(a->chan, 42, &a->val, sizeof(a->val), 1000);
    return NULL;
}

static void test_channel_wait_policies(void)
{
    printf("Testing cobalt_shm_channel wait policies (SPIN, BLOCKING, ADAPTIVE)...\n");

    /* 1. SPIN policy with timeout 0 and positive timeout */
    const char         *name_spin = "/cobalt_test_wait_spin";
    cobalt_shm_config_t cfg_spin  = {
        .name              = name_spin,
        .mode              = COBALT_SHM_MODE_SPSC,
        .wait_policy       = COBALT_SHM_WAIT_SPIN,
        .capacity          = 1024,
        .slot_size         = 0,
        .create_if_missing = true,
        .spin_threshold    = 100,
    };
    cobalt_shm_unlink(name_spin);
    cobalt_shm_channel_t *chan_spin = cobalt_shm_channel_create(&cfg_spin);
    TEST_ASSERT(chan_spin != NULL);

    char     buf[16];
    uint16_t type    = 0;
    size_t   out_len = 0;
    int      rc      = cobalt_shm_channel_recv(chan_spin, &type, buf, sizeof(buf), &out_len, 0);
    TEST_ASSERT(rc == COBALT_ERROR_EMPTY_CONTAINER);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    rc = cobalt_shm_channel_recv(chan_spin, &type, buf, sizeof(buf), &out_len, 15);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    TEST_ASSERT(rc == COBALT_ERROR_TIMEOUT);
    long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000L + (t1.tv_nsec - t0.tv_nsec) / 1000000L;
    TEST_ASSERT(elapsed_ms >= 10);
    cobalt_shm_channel_destroy(chan_spin);

    /* 2. BLOCKING policy with timeout */
    const char         *name_block = "/cobalt_test_wait_block";
    cobalt_shm_config_t cfg_block  = {
        .name              = name_block,
        .mode              = COBALT_SHM_MODE_SPSC,
        .wait_policy       = COBALT_SHM_WAIT_BLOCKING,
        .capacity          = 1024,
        .slot_size         = 0,
        .create_if_missing = true,
        .spin_threshold    = 100,
    };
    cobalt_shm_unlink(name_block);
    cobalt_shm_channel_t *chan_block = cobalt_shm_channel_create(&cfg_block);
    TEST_ASSERT(chan_block != NULL);

    clock_gettime(CLOCK_MONOTONIC, &t0);
    rc = cobalt_shm_channel_recv(chan_block, &type, buf, sizeof(buf), &out_len, 20);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    TEST_ASSERT(rc == COBALT_ERROR_TIMEOUT);
    elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000L + (t1.tv_nsec - t0.tv_nsec) / 1000000L;
    TEST_ASSERT(elapsed_ms >= 15);

    /* 3. ADAPTIVE policy with timeout */
    const char         *name_adapt = "/cobalt_test_wait_adapt";
    cobalt_shm_config_t cfg_adapt  = {
        .name              = name_adapt,
        .mode              = COBALT_SHM_MODE_SPSC,
        .wait_policy       = COBALT_SHM_WAIT_ADAPTIVE,
        .capacity          = 1024,
        .slot_size         = 0,
        .create_if_missing = true,
        .spin_threshold    = 500,
    };
    cobalt_shm_unlink(name_adapt);
    cobalt_shm_channel_t *chan_adapt = cobalt_shm_channel_create(&cfg_adapt);
    TEST_ASSERT(chan_adapt != NULL);

    clock_gettime(CLOCK_MONOTONIC, &t0);
    rc = cobalt_shm_channel_recv(chan_adapt, &type, buf, sizeof(buf), &out_len, 20);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    TEST_ASSERT(rc == COBALT_ERROR_TIMEOUT);
    elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000L + (t1.tv_nsec - t0.tv_nsec) / 1000000L;
    TEST_ASSERT(elapsed_ms >= 15);
    cobalt_shm_channel_destroy(chan_adapt);

    /* 4. Cross-thread notification wakeup under BLOCKING */
    cobalt_shm_channel_t *client_block =
        cobalt_shm_channel_open(name_block, COBALT_SHM_WAIT_BLOCKING);
    TEST_ASSERT(client_block != NULL);

    delayed_sender_arg_t s_arg = {
        .chan     = client_block,
        .sleep_ms = 30,
        .val      = 12345,
    };
    cobalt_thread_t s_thread;
    int             trc = cobalt_thread_create(delayed_sender_worker, &s_arg, &s_thread);
    TEST_ASSERT(trc == 0);

    int recv_val = 0;
    rc = cobalt_shm_channel_recv(chan_block, &type, &recv_val, sizeof(recv_val), &out_len, 1000);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(type == 42);
    TEST_ASSERT(out_len == sizeof(int));
    TEST_ASSERT(recv_val == 12345);

    cobalt_thread_join(s_thread);

    cobalt_shm_channel_destroy(client_block);
    cobalt_shm_channel_destroy(chan_block);

    printf("  Wait policies test passed\n");
}

typedef struct {
    int                 count;
    int                 expected_count;
    cobalt_eventloop_t *loop;
} ev_channel_test_ctx_t;

static void on_ev_channel_msg(
    cobalt_shm_channel_t *chan, uint16_t msg_type, const void *payload, size_t len, void *user_data)
{
    (void)chan;
    ev_channel_test_ctx_t *ctx = (ev_channel_test_ctx_t *)user_data;
    TEST_ASSERT(msg_type == (uint16_t)(200 + ctx->count));
    TEST_ASSERT(len == sizeof(int));
    int val = *(const int *)payload;
    TEST_ASSERT(val == 1000 + ctx->count);
    ctx->count++;
    if (ctx->count >= ctx->expected_count) {
        cobalt_eventloop_stop(ctx->loop);
    }
}

static void on_ev_safety_timer(uint64_t timer_id, void *user_data)
{
    (void)timer_id;
    cobalt_eventloop_t *loop = (cobalt_eventloop_t *)user_data;
    cobalt_eventloop_stop(loop);
}

static void test_channel_eventloop_integration(void)
{
    printf("Testing cobalt_shm_channel EventLoop integration...\n");

    const char *name = "/cobalt_test_shm_chan_ev";
    cobalt_shm_unlink(name);

    cobalt_shm_config_t cfg = {
        .name              = name,
        .mode              = COBALT_SHM_MODE_SPSC,
        .wait_policy       = COBALT_SHM_WAIT_BLOCKING,
        .capacity          = 4096,
        .slot_size         = 0,
        .create_if_missing = true,
        .spin_threshold    = 1000,
    };

    cobalt_shm_channel_t *server = cobalt_shm_channel_create(&cfg);
    TEST_ASSERT(server != NULL);

    cobalt_shm_channel_t *client = cobalt_shm_channel_open(name, COBALT_SHM_WAIT_BLOCKING);
    TEST_ASSERT(client != NULL);

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    ev_channel_test_ctx_t ctx = {
        .count          = 0,
        .expected_count = 10,
        .loop           = loop,
    };

    int rc = cobalt_shm_channel_attach_eventloop(server, loop, on_ev_channel_msg, &ctx);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    /* Send 10 messages from client handle */
    for (int i = 0; i < 10; i++) {
        int val = 1000 + i;
        rc      = cobalt_shm_channel_send(client, (uint16_t)(200 + i), &val, sizeof(val), 100);
        TEST_ASSERT(rc == COBALT_SUCCESS);
    }

    /* Safety timer in case eventloop hangs */
    uint64_t timer_id = cobalt_eventloop_add_timer(loop, 2000, 0, on_ev_safety_timer, loop);
    TEST_ASSERT(timer_id > 0);

    cobalt_eventloop_run(loop);

    TEST_ASSERT(ctx.count == 10);

    rc = cobalt_shm_channel_detach_eventloop(server);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_eventloop_del_timer(loop, timer_id);
    cobalt_eventloop_destroy(loop);
    cobalt_shm_channel_destroy(client);
    cobalt_shm_channel_destroy(server);

    printf("  EventLoop integration test passed\n");
}

static void test_channel_send_timeout_full(void)
{
    printf("Testing cobalt_shm_channel send timeout on full queue...\n");

    const char         *name = "/cobalt_test_chan_full";
    cobalt_shm_config_t cfg  = {
        .name              = name,
        .mode              = COBALT_SHM_MODE_SPSC,
        .wait_policy       = COBALT_SHM_WAIT_BLOCKING,
        .capacity          = 512,
        .slot_size         = 0,
        .create_if_missing = true,
        .spin_threshold    = 100,
    };
    cobalt_shm_unlink(name);

    cobalt_shm_channel_t *chan = cobalt_shm_channel_create(&cfg);
    TEST_ASSERT(chan != NULL);

    char payload[64];
    memset(payload, 'A', sizeof(payload));

    /* Fill until full */
    int rc = COBALT_SUCCESS;
    while ((rc = cobalt_shm_channel_send(chan, 1, payload, sizeof(payload), 0)) == COBALT_SUCCESS) {
    }
    TEST_ASSERT(rc == COBALT_ERROR_QUEUE_FULL);

    /* Timeout test with positive timeout */
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    rc = cobalt_shm_channel_send(chan, 1, payload, sizeof(payload), 20);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    TEST_ASSERT(rc == COBALT_ERROR_TIMEOUT);
    long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000L + (t1.tv_nsec - t0.tv_nsec) / 1000000L;
    TEST_ASSERT(elapsed_ms >= 15);

    /* Consume one item to allow writing again */
    char     buf[64];
    uint16_t type    = 0;
    size_t   out_len = 0;
    rc               = cobalt_shm_channel_recv(chan, &type, buf, sizeof(buf), &out_len, 0);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    rc = cobalt_shm_channel_send(chan, 2, payload, sizeof(payload), 0);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_shm_channel_destroy(chan);
    printf("  Send timeout full test passed\n");
}

static void on_ev_mpmc_msg(
    cobalt_shm_channel_t *chan, uint16_t msg_type, const void *payload, size_t len, void *user_data)
{
    (void)chan;
    (void)msg_type;
    ev_channel_test_ctx_t *ctx = (ev_channel_test_ctx_t *)user_data;
    if (ctx && payload && len == sizeof(int)) {
        int val = *(const int *)payload;
        TEST_ASSERT(val >= 500 && val < 505);
        ctx->count++;
        if (ctx->count >= ctx->expected_count) {
            cobalt_eventloop_stop(ctx->loop);
        }
    }
}

static void test_channel_mpmc_eventloop_integration(void)
{
    printf("Testing cobalt_shm_channel MPMC EventLoop integration...\n");

    const char         *name = "/cobalt_test_chan_ev_mpmc";
    cobalt_shm_config_t cfg  = {
        .name              = name,
        .mode              = COBALT_SHM_MODE_MPMC,
        .wait_policy       = COBALT_SHM_WAIT_BLOCKING,
        .capacity          = 16,
        .slot_size         = 64,
        .create_if_missing = true,
        .spin_threshold    = 1000,
    };
    cobalt_shm_unlink(name);

    cobalt_shm_channel_t *server = cobalt_shm_channel_create(&cfg);
    TEST_ASSERT(server != NULL);

    cobalt_shm_channel_t *client = cobalt_shm_channel_open(name, COBALT_SHM_WAIT_BLOCKING);
    TEST_ASSERT(client != NULL);

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    ev_channel_test_ctx_t ctx = {
        .count          = 0,
        .expected_count = 5,
        .loop           = loop,
    };

    int rc = cobalt_shm_channel_attach_eventloop(server, loop, on_ev_mpmc_msg, &ctx);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    for (int i = 0; i < 5; i++) {
        int val = 500 + i;
        rc      = cobalt_shm_channel_send(client, 0, &val, sizeof(val), 100);
        TEST_ASSERT(rc == COBALT_SUCCESS);
    }

    uint64_t timer_id = cobalt_eventloop_add_timer(loop, 2000, 0, on_ev_safety_timer, loop);
    TEST_ASSERT(timer_id > 0);

    cobalt_eventloop_run(loop);

    TEST_ASSERT(ctx.count == 5);

    rc = cobalt_shm_channel_detach_eventloop(server);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_eventloop_del_timer(loop, timer_id);
    cobalt_eventloop_destroy(loop);
    cobalt_shm_channel_destroy(client);
    cobalt_shm_channel_destroy(server);

    printf("  MPMC EventLoop integration test passed\n");
}

static void test_channel_invalid_args(void)
{
    printf("Testing cobalt_shm_channel invalid arguments and error paths...\n");

    /* NULL configs */
    TEST_ASSERT(cobalt_shm_channel_create(NULL) == NULL);

    cobalt_shm_config_t bad_cfg = {
        .name              = NULL,
        .mode              = COBALT_SHM_MODE_SPSC,
        .capacity          = 1024,
        .create_if_missing = true,
    };
    TEST_ASSERT(cobalt_shm_channel_create(&bad_cfg) == NULL);

    /* Non power of 2 capacity */
    bad_cfg.name     = "/cobalt_bad_cap";
    bad_cfg.capacity = 1000;
    TEST_ASSERT(cobalt_shm_channel_create(&bad_cfg) == NULL);

    /* Bad mode */
    bad_cfg.capacity = 1024;
    bad_cfg.mode     = (cobalt_shm_mode_t)99;
    TEST_ASSERT(cobalt_shm_channel_create(&bad_cfg) == NULL);

    /* Open non-existent channel */
    TEST_ASSERT(cobalt_shm_channel_open("/non_existent_chan_9999", COBALT_SHM_WAIT_SPIN) == NULL);
    TEST_ASSERT(cobalt_shm_channel_open(NULL, COBALT_SHM_WAIT_SPIN) == NULL);

    /* Getters on NULL */
    TEST_ASSERT(cobalt_shm_channel_get_mode(NULL) == COBALT_SHM_MODE_UNKNOWN);
    TEST_ASSERT(cobalt_shm_channel_get_wait_policy(NULL) == COBALT_SHM_WAIT_SPIN);
    TEST_ASSERT(cobalt_shm_channel_get_read_fd(NULL) == -1);

    /* Destroy NULL is safe no-op */
    cobalt_shm_channel_destroy(NULL);

    printf("  Channel invalid arguments test passed\n");
}

#ifndef _WIN32
static void cleanup_shm_and_notif(const char *name)
{
    if (!name) {
        return;
    }
    cobalt_shm_unlink(name);
    char        path[256];
    const char *p = name;
    while (*p == '/') {
        p++;
    }
    snprintf(path, sizeof(path), "/dev/shm/%s_notif", p);
    unlink(path);
    snprintf(path, sizeof(path), "/tmp/%s_notif", p);
    unlink(path);
}

static uint32_t g_crc32_table[256];
static bool     g_crc32_table_init = false;

static void init_crc32_table(void)
{
    if (g_crc32_table_init) {
        return;
    }
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
        }
        g_crc32_table[i] = c;
    }
    g_crc32_table_init = true;
}

static uint32_t calc_crc32(const uint8_t *data, size_t len)
{
    init_crc32_table();
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) {
        crc = g_crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

typedef struct {
    uint32_t index;
    uint32_t crc;
} fork_spsc_msg_hdr_t;

static void test_shm_fork_spsc_stream(void)
{
    printf("Testing cobalt_shm fork SPSC stream (50,000 variable length messages + CRC32)...\n");

    const char *name = "/cobalt_fork_spsc";
    cleanup_shm_and_notif(name);

    cobalt_shm_config_t cfg = {
        .name              = name,
        .mode              = COBALT_SHM_MODE_SPSC,
        .wait_policy       = COBALT_SHM_WAIT_ADAPTIVE,
        .capacity          = 65536,
        .slot_size         = 0,
        .create_if_missing = true,
        .spin_threshold    = 2000,
    };

    cobalt_shm_channel_t *server = cobalt_shm_channel_create(&cfg);
    TEST_ASSERT(server != NULL);

    fflush(stdout);
    fflush(stderr);

    pid_t child_pid = fork();
    TEST_ASSERT(child_pid >= 0);

    if (child_pid == 0) {
        /* Child process: sender */
        cobalt_shm_channel_t *client = cobalt_shm_channel_open(name, COBALT_SHM_WAIT_ADAPTIVE);
        if (!client) {
            _exit(1);
        }

        uint8_t msg_buf[512];
        for (int i = 0; i < 50000; i++) {
            size_t               len = 16 + (size_t)((i * 31) % (256 - 16 + 1));
            fork_spsc_msg_hdr_t *hdr = (fork_spsc_msg_hdr_t *)msg_buf;
            hdr->index               = (uint32_t)i;

            size_t   payload_len = len - sizeof(fork_spsc_msg_hdr_t);
            uint8_t *payload     = msg_buf + sizeof(fork_spsc_msg_hdr_t);
            for (size_t j = 0; j < payload_len; j++) {
                payload[j] = (uint8_t)((i + j * 7) & 0xFF);
            }
            hdr->crc = calc_crc32(payload, payload_len);

            int send_retries = 0;
            int rc           = COBALT_ERROR_TIMEOUT;
            while (rc == COBALT_ERROR_TIMEOUT && send_retries++ < 50) {
                rc = cobalt_shm_channel_send(client, (uint16_t)(i % 5000 + 1), msg_buf, len, 100);
            }
            if (rc != COBALT_SUCCESS) {
                _exit(2);
            }
        }

        cobalt_shm_channel_destroy(client);
        _exit(0);
    }

    /* Parent process: receiver */
    for (int i = 0; i < 50000; i++) {
        uint8_t  recv_buf[512];
        uint16_t type         = 0;
        size_t   out_len      = 0;
        int      recv_retries = 0;
        int      rc           = COBALT_ERROR_TIMEOUT;
        while (rc == COBALT_ERROR_TIMEOUT && recv_retries++ < 50) {
            rc = cobalt_shm_channel_recv(server, &type, recv_buf, sizeof(recv_buf), &out_len, 100);
        }
        TEST_ASSERT(rc == COBALT_SUCCESS);
        TEST_ASSERT(out_len >= sizeof(fork_spsc_msg_hdr_t));
        TEST_ASSERT(type == (uint16_t)(i % 5000 + 1));

        fork_spsc_msg_hdr_t *hdr = (fork_spsc_msg_hdr_t *)recv_buf;
        TEST_ASSERT(hdr->index == (uint32_t)i);

        size_t   payload_len  = out_len - sizeof(fork_spsc_msg_hdr_t);
        uint8_t *payload      = recv_buf + sizeof(fork_spsc_msg_hdr_t);
        uint32_t expected_crc = calc_crc32(payload, payload_len);
        TEST_ASSERT(hdr->crc == expected_crc);
    }

    int   status = 0;
    pid_t waited = waitpid(child_pid, &status, 0);
    TEST_ASSERT(waited == child_pid);
    TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    cobalt_shm_channel_destroy(server);
    cleanup_shm_and_notif(name);

    printf("  Fork SPSC pingpong test passed\n");
}

typedef struct {
    uint32_t producer_id;
    uint32_t seq;
    uint32_t payload[14]; /* Total 64 bytes */
} fork_mpmc_item_t;

typedef struct {
    _Atomic uint32_t total_consumed;
    _Atomic uint8_t  received[2][25000];
} fork_mpmc_tracker_t;

static void test_shm_fork_mpmc_contention(void)
{
    printf("Testing cobalt_shm fork MPMC contention (2P + 2C, 50,000 items)...\n");

    const char *chan_name    = "/cobalt_fork_mpmc";
    const char *counter_name = "/cobalt_fork_mpmc_counter";

    cleanup_shm_and_notif(chan_name);
    cleanup_shm_and_notif(counter_name);

    cobalt_shm_config_t cfg = {
        .name              = chan_name,
        .mode              = COBALT_SHM_MODE_MPMC,
        .wait_policy       = COBALT_SHM_WAIT_BLOCKING,
        .capacity          = 1024,
        .slot_size         = 64,
        .create_if_missing = true,
        .spin_threshold    = 1000,
    };

    cobalt_shm_channel_t *server = cobalt_shm_channel_create(&cfg);
    TEST_ASSERT(server != NULL);

    cobalt_shm_t *tracker_shm = NULL;
    int           rc = cobalt_shm_create(counter_name, sizeof(fork_mpmc_tracker_t), &tracker_shm);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(tracker_shm != NULL);

    fork_mpmc_tracker_t *tracker = (fork_mpmc_tracker_t *)cobalt_shm_get_addr(tracker_shm);
    TEST_ASSERT(tracker != NULL);
    memset(tracker, 0, sizeof(*tracker));

    fflush(stdout);
    fflush(stderr);

    pid_t pids[4];

    /* Fork 2 producers */
    for (int p = 0; p < 2; p++) {
        pids[p] = fork();
        TEST_ASSERT(pids[p] >= 0);
        if (pids[p] == 0) {
            cobalt_shm_channel_t *client =
                cobalt_shm_channel_open(chan_name, COBALT_SHM_WAIT_BLOCKING);
            if (!client) {
                _exit(1);
            }

            for (uint32_t s = 0; s < 25000; s++) {
                fork_mpmc_item_t item;
                item.producer_id = (uint32_t)p;
                item.seq         = s;
                item.payload[0]  = (uint32_t)(p * 100000 + s);
                memset(item.payload + 1, 0, sizeof(item.payload) - sizeof(uint32_t));

                int send_retries = 0;
                int send_rc      = COBALT_ERROR_TIMEOUT;
                while (send_rc == COBALT_ERROR_TIMEOUT && send_retries++ < 50) {
                    send_rc = cobalt_shm_channel_send(client, 0, &item, sizeof(item), 100);
                }
                if (send_rc != COBALT_SUCCESS) {
                    _exit(2);
                }
            }

            cobalt_shm_channel_destroy(client);
            _exit(0);
        }
    }

    /* Fork 2 consumers */
    for (int c = 0; c < 2; c++) {
        pids[2 + c] = fork();
        TEST_ASSERT(pids[2 + c] >= 0);
        if (pids[2 + c] == 0) {
            cobalt_shm_channel_t *client =
                cobalt_shm_channel_open(chan_name, COBALT_SHM_WAIT_BLOCKING);
            if (!client) {
                _exit(10);
            }

            cobalt_shm_t *c_shm = NULL;
            if (cobalt_shm_open(counter_name, &c_shm) != COBALT_SUCCESS) {
                _exit(11);
            }
            fork_mpmc_tracker_t *c_tracker = (fork_mpmc_tracker_t *)cobalt_shm_get_addr(c_shm);
            if (!c_tracker) {
                _exit(12);
            }

            int consecutive_timeouts = 0;
            while (atomic_load_explicit(&c_tracker->total_consumed, memory_order_acquire) < 50000) {
                fork_mpmc_item_t item;
                size_t           out_len = 0;
                int              recv_rc =
                    cobalt_shm_channel_recv(client, NULL, &item, sizeof(item), &out_len, 100);
                if (recv_rc == COBALT_SUCCESS) {
                    consecutive_timeouts = 0;
                    if (out_len != sizeof(item)) {
                        _exit(13);
                    }
                    if (item.producer_id >= 2 || item.seq >= 25000) {
                        _exit(14);
                    }
                    if (item.payload[0] != (item.producer_id * 100000 + item.seq)) {
                        _exit(15);
                    }
                    uint8_t prev = atomic_exchange_explicit(
                        &c_tracker->received[item.producer_id][item.seq], 1, memory_order_acq_rel);
                    if (prev != 0) {
                        _exit(16);
                    }
                    uint32_t cur = atomic_fetch_add_explicit(
                        &c_tracker->total_consumed, 1, memory_order_release);
                    if (cur + 1 >= 50000) {
                        break;
                    }
                } else if (recv_rc == COBALT_ERROR_TIMEOUT ||
                           recv_rc == COBALT_ERROR_EMPTY_CONTAINER) {
                    if (atomic_load_explicit(&c_tracker->total_consumed, memory_order_acquire) >=
                        50000) {
                        break;
                    }
                    if (++consecutive_timeouts > 100) {
                        _exit(18);
                    }
                } else {
                    _exit(17);
                }
            }

            cobalt_shm_close(c_shm);
            cobalt_shm_channel_destroy(client);
            _exit(0);
        }
    }

    /* Wait for all 4 children */
    for (int i = 0; i < 4; i++) {
        int   status = 0;
        pid_t waited = waitpid(pids[i], &status, 0);
        TEST_ASSERT(waited == pids[i]);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr,
                    "Child pids[%d]=%d exited with status=%d (WIFEXITED=%d, WEXITSTATUS=%d, "
                    "WIFSIGNALED=%d, WTERMSIG=%d)\n",
                    i,
                    (int)pids[i],
                    status,
                    WIFEXITED(status),
                    WEXITSTATUS(status),
                    WIFSIGNALED(status),
                    WTERMSIG(status));
        }
        TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }

    TEST_ASSERT(atomic_load_explicit(&tracker->total_consumed, memory_order_acquire) == 50000);
    for (int p = 0; p < 2; p++) {
        for (int s = 0; s < 25000; s++) {
            TEST_ASSERT(atomic_load_explicit(&tracker->received[p][s], memory_order_acquire) == 1);
        }
    }

    cobalt_shm_close(tracker_shm);
    cleanup_shm_and_notif(counter_name);
    cobalt_shm_channel_destroy(server);
    cleanup_shm_and_notif(chan_name);

    printf("  Fork MPMC contention test passed\n");
}

typedef struct {
    int                 count;
    int                 expected_count;
    cobalt_eventloop_t *loop;
    bool                corrupted;
} fork_ev_ctx_t;

static void on_fork_ev_msg(
    cobalt_shm_channel_t *chan, uint16_t msg_type, const void *payload, size_t len, void *user_data)
{
    (void)chan;
    fork_ev_ctx_t *ctx = (fork_ev_ctx_t *)user_data;
    if (len != sizeof(uint32_t)) {
        ctx->corrupted = true;
        cobalt_eventloop_stop(ctx->loop);
        return;
    }
    uint32_t val = *(const uint32_t *)payload;
    if (val != (uint32_t)ctx->count || msg_type != (uint16_t)(ctx->count % 1000 + 1)) {
        ctx->corrupted = true;
        cobalt_eventloop_stop(ctx->loop);
        return;
    }
    ctx->count++;
    if (ctx->count >= ctx->expected_count) {
        cobalt_eventloop_stop(ctx->loop);
    }
}

static void test_shm_fork_eventloop_drain(void)
{
    printf("Testing cobalt_shm fork EventLoop drain (10,000 messages)...\n");

    const char *name = "/cobalt_fork_ev";
    cleanup_shm_and_notif(name);

    cobalt_shm_config_t cfg = {
        .name              = name,
        .mode              = COBALT_SHM_MODE_SPSC,
        .wait_policy       = COBALT_SHM_WAIT_BLOCKING,
        .capacity          = 65536,
        .slot_size         = 0,
        .create_if_missing = true,
        .spin_threshold    = 1000,
    };

    cobalt_shm_channel_t *server = cobalt_shm_channel_create(&cfg);
    TEST_ASSERT(server != NULL);

    cobalt_eventloop_t *loop = cobalt_eventloop_create();
    TEST_ASSERT(loop != NULL);

    fork_ev_ctx_t ctx = {
        .count          = 0,
        .expected_count = 10000,
        .loop           = loop,
        .corrupted      = false,
    };

    int rc = cobalt_shm_channel_attach_eventloop(server, loop, on_fork_ev_msg, &ctx);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    fflush(stdout);
    fflush(stderr);

    pid_t child_pid = fork();
    TEST_ASSERT(child_pid >= 0);

    if (child_pid == 0) {
        /* Child sends 10,000 messages */
        cobalt_shm_channel_t *client = cobalt_shm_channel_open(name, COBALT_SHM_WAIT_BLOCKING);
        if (!client) {
            _exit(1);
        }

        for (uint32_t i = 0; i < 10000; i++) {
            int send_retries = 0;
            int send_rc      = COBALT_ERROR_TIMEOUT;
            while (send_rc == COBALT_ERROR_TIMEOUT && send_retries++ < 50) {
                send_rc =
                    cobalt_shm_channel_send(client, (uint16_t)(i % 1000 + 1), &i, sizeof(i), 100);
            }
            if (send_rc != COBALT_SUCCESS) {
                _exit(2);
            }
        }

        cobalt_shm_channel_destroy(client);
        _exit(0);
    }

    uint64_t timer_id = cobalt_eventloop_add_timer(loop, 10000, 0, on_ev_safety_timer, loop);
    TEST_ASSERT(timer_id > 0);

    cobalt_eventloop_run(loop);

    TEST_ASSERT(!ctx.corrupted);
    TEST_ASSERT(ctx.count == 10000);

    int   status = 0;
    pid_t waited = waitpid(child_pid, &status, 0);
    TEST_ASSERT(waited == child_pid);
    TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    rc = cobalt_shm_channel_detach_eventloop(server);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    cobalt_eventloop_del_timer(loop, timer_id);
    cobalt_eventloop_destroy(loop);
    cobalt_shm_channel_destroy(server);
    cleanup_shm_and_notif(name);

    printf("  Fork EventLoop drain test passed\n");
}

static void test_shm_fork_crash_recovery(void)
{
    printf("Testing cobalt_shm fork crash recovery (SIGKILL during uncommitted reserve)...\n");

    const char *name = "/cobalt_fork_crash";
    cleanup_shm_and_notif(name);

    cobalt_shm_config_t cfg = {
        .name              = name,
        .mode              = COBALT_SHM_MODE_SPSC,
        .wait_policy       = COBALT_SHM_WAIT_BLOCKING,
        .capacity          = 4096,
        .slot_size         = 0,
        .create_if_missing = true,
        .spin_threshold    = 1000,
    };

    cobalt_shm_channel_t *server = cobalt_shm_channel_create(&cfg);
    TEST_ASSERT(server != NULL);

    int sync_pipe[2];
    int prc = pipe(sync_pipe);
    TEST_ASSERT(prc == 0);

    fflush(stdout);
    fflush(stderr);

    pid_t child_pid = fork();
    TEST_ASSERT(child_pid >= 0);

    if (child_pid == 0) {
        close(sync_pipe[0]);
#ifdef __linux__
        prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif

        cobalt_shm_channel_t *client = cobalt_shm_channel_open(name, COBALT_SHM_WAIT_BLOCKING);
        if (!client) {
            _exit(1);
        }

        void *wptr = NULL;
        int   rc   = cobalt_shm_channel_reserve(client, 128, &wptr);
        if (rc != COBALT_SUCCESS || !wptr) {
            _exit(2);
        }

        /* Write payload into reserved slot but do NOT commit */
        memset(wptr, 0xEE, 128);

        /* Signal parent that reserve was completed */
        if (write(sync_pipe[1], "R", 1) != 1) {
            _exit(3);
        }
        close(sync_pipe[1]);

        /* Block awaiting kill */
        while (1) {
            pause();
        }
    }

    close(sync_pipe[1]);
    char    sig = 0;
    ssize_t n   = read(sync_pipe[0], &sig, 1);
    close(sync_pipe[0]);
    TEST_ASSERT(n == 1 && sig == 'R');

    /* Kill child abruptly with SIGKILL */
    int krc = kill(child_pid, SIGKILL);
    TEST_ASSERT(krc == 0);

    int   status = 0;
    pid_t waited = waitpid(child_pid, &status, 0);
    TEST_ASSERT(waited == child_pid);
    TEST_ASSERT(WIFSIGNALED(status));
    TEST_ASSERT(WTERMSIG(status) == SIGKILL);

    /* Verify channel remains completely usable */
    /* 1. Uncommitted reserved slot was never committed, so container is empty */
    char     buf[256];
    uint16_t type    = 0;
    size_t   out_len = 0;
    int      rc      = cobalt_shm_channel_recv(server, &type, buf, sizeof(buf), &out_len, 0);
    TEST_ASSERT(rc == COBALT_ERROR_EMPTY_CONTAINER);

    /* 2. Server can send and receive new messages */
    for (int i = 0; i < 100; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "crash-recovery-%03d", i);
        rc = cobalt_shm_channel_send(server, (uint16_t)(500 + i), msg, strlen(msg) + 1, 1000);
        TEST_ASSERT(rc == COBALT_SUCCESS);

        char     recv_buf[64];
        uint16_t recv_type = 0;
        size_t   recv_len  = 0;
        rc                 = cobalt_shm_channel_recv(
            server, &recv_type, recv_buf, sizeof(recv_buf), &recv_len, 1000);
        TEST_ASSERT(rc == COBALT_SUCCESS);
        TEST_ASSERT(recv_type == (uint16_t)(500 + i));
        TEST_ASSERT(recv_len == strlen(msg) + 1);
        TEST_ASSERT(strcmp(recv_buf, msg) == 0);
    }

    /* 3. A fresh client handle can connect and exchange messages with server */
    cobalt_shm_channel_t *client = cobalt_shm_channel_open(name, COBALT_SHM_WAIT_BLOCKING);
    TEST_ASSERT(client != NULL);

    const char *test_msg = "post_crash_client_msg";
    rc = cobalt_shm_channel_send(client, 999, test_msg, strlen(test_msg) + 1, 1000);
    TEST_ASSERT(rc == COBALT_SUCCESS);

    char     client_recv[64];
    uint16_t client_type = 0;
    size_t   client_len  = 0;
    rc                   = cobalt_shm_channel_recv(
        server, &client_type, client_recv, sizeof(client_recv), &client_len, 1000);
    TEST_ASSERT(rc == COBALT_SUCCESS);
    TEST_ASSERT(client_type == 999);
    TEST_ASSERT(client_len == strlen(test_msg) + 1);
    TEST_ASSERT(strcmp(client_recv, test_msg) == 0);

    cobalt_shm_channel_destroy(client);
    cobalt_shm_channel_destroy(server);
    cleanup_shm_and_notif(name);

    printf("  Fork crash recovery test passed\n");
}
#endif

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
    printf("  MPMC shared memory ring tests completed successfully!\n\n");

    printf("Testing shared memory channel facade (cobalt_shm_channel_t)...\n");
    test_channel_spsc_lifecycle_and_send_recv();
    test_channel_mpmc_lifecycle_and_send_recv();
    test_channel_wait_policies();
    test_channel_eventloop_integration();
    test_channel_send_timeout_full();
    test_channel_mpmc_eventloop_integration();
    test_channel_invalid_args();
    printf("  Shared memory channel facade tests completed successfully!\n\n");

#ifndef _WIN32
    printf("Testing multi-process fork stress and crash recovery...\n");
    test_shm_fork_spsc_stream();
    test_shm_fork_mpmc_contention();
    test_shm_fork_eventloop_drain();
    test_shm_fork_crash_recovery();
    printf("  Multi-process stress and crash recovery tests completed successfully!\n");
#endif
}
