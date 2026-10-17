/**
 * @file test_shm.c
 * @brief Unit tests for platform shared memory primitive (cobalt_shm_t).
 */

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

void test_shm(void)
{
    printf("Testing shared memory primitive (cobalt_shm_t)...\n");
    test_shm_create_and_close();
    test_shm_open_existing();
    test_shm_invalid_args();
    test_shm_name_formatting();
    printf("  Shared memory tests completed successfully!\n");
}
