/**
 * @file test_platform_utils.c
 * @brief Unit test for platform utility macros and functions.
 */

#include "cobalt/platform/utils.h"
#include "test_framework.h"
#include <stdio.h>

void test_align_macro(void)
{
    printf("Testing cobalt_align macro...\n");

    TEST_ASSERT(cobalt_align(0, 8) == 0);
    TEST_ASSERT(cobalt_align(1, 8) == 8);
    TEST_ASSERT(cobalt_align(7, 8) == 8);
    TEST_ASSERT(cobalt_align(8, 8) == 8);
    TEST_ASSERT(cobalt_align(9, 8) == 16);
    TEST_ASSERT(cobalt_align(100, 16) == 112);
    TEST_ASSERT(cobalt_align(128, 16) == 128);
    TEST_ASSERT(cobalt_align(1, 64) == 64);

    printf("  Alignment macro: OK\n");
}

void test_align_offset(void)
{
    printf("Testing cobalt_align_offset macro...\n");

    TEST_ASSERT(cobalt_align_offset(0, 8) == 0);
    TEST_ASSERT(cobalt_align_offset(1, 8) == 7);
    TEST_ASSERT(cobalt_align_offset(8, 8) == 0);
    TEST_ASSERT(cobalt_align_offset(9, 8) == 7);

    printf("  Alignment offset: OK\n");
}

void test_is_aligned(void)
{
    printf("Testing cobalt_is_aligned macro...\n");

    TEST_ASSERT(cobalt_is_aligned(0, 8) == 1);
    TEST_ASSERT(cobalt_is_aligned(8, 8) == 1);
    TEST_ASSERT(cobalt_is_aligned(16, 8) == 1);
    TEST_ASSERT(cobalt_is_aligned(1, 8) == 0);
    TEST_ASSERT(cobalt_is_aligned(7, 8) == 0);
    TEST_ASSERT(cobalt_is_aligned(9, 8) == 0);

    printf("  Is aligned: OK\n");
}

void test_byte_swap(void)
{
    printf("Testing byte swap functions...\n");

    TEST_ASSERT(cobalt_swap16(0x1234) == 0x3412);
    TEST_ASSERT(cobalt_swap16(0x00FF) == 0xFF00);
    TEST_ASSERT(cobalt_swap16(0xFF00) == 0x00FF);

    TEST_ASSERT(cobalt_swap32(0x12345678) == 0x78563412);
    TEST_ASSERT(cobalt_swap32(0x000000FF) == 0xFF000000);

    TEST_ASSERT(cobalt_swap64(0x0102030405060708ULL) == 0x0807060504030201ULL);

    printf("  Byte swap: OK\n");
}

void test_endian_host_net(void)
{
    printf("Testing host<->network conversion...\n");

    /* On little-endian: host_to_net swaps, net_to_host swaps back */
    uint16_t h16 = 0x1234;
    uint16_t n16 = cobalt_host_to_net16(h16);
    TEST_ASSERT(cobalt_net_to_host16(n16) == h16);

    uint32_t h32 = 0x12345678;
    uint32_t n32 = cobalt_host_to_net32(h32);
    TEST_ASSERT(cobalt_net_to_host32(n32) == h32);

    uint64_t h64 = 0x0102030405060708ULL;
    uint64_t n64 = cobalt_host_to_net64(h64);
    TEST_ASSERT(cobalt_net_to_host64(n64) == h64);

    printf("  Host<->Network round-trip: OK\n");
}

void test_endian_idempotent(void)
{
    printf("Testing idempotency (swap twice = identity)...\n");

    TEST_ASSERT(cobalt_swap16(cobalt_swap16(0xABCD)) == 0xABCD);
    TEST_ASSERT(cobalt_swap32(cobalt_swap32(0x12345678)) == 0x12345678);
    TEST_ASSERT(cobalt_swap64(cobalt_swap64(0x0102030405060708ULL)) == 0x0102030405060708ULL);

    printf("  Idempotency: OK\n");
}

void test_platform_utils(void)
{
    printf("Testing platform utils...\n");
    test_align_macro();
    test_align_offset();
    test_is_aligned();
    test_byte_swap();
    test_endian_host_net();
    test_endian_idempotent();
    printf("  Platform utils tests completed\n");
}

