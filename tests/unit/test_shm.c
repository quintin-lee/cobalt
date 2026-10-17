/**
 * @file test_shm.c
 * @brief Unit tests for platform shared memory primitive (cobalt_shm_t).
 */

#include "cobalt/platform/notifier.h"
#include "cobalt/platform/shm.h"
#include "cobalt/runtime/error.h"
#include "test_framework.h"

#include <stdint.h>
#include <stdio.h>
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
    printf("  Notification tests completed successfully!\n");
}
