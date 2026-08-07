/**
 * @file test_slab.c
 * @brief Unit test for slab allocator.
 */

#include "cobalt/memory/slab.h"
#include "test_framework.h"
#include <stdint.h>
#include <stdio.h>

void test_slab_basic(void)
{
    printf("Testing slab basic alloc/free...\n");

    size_t         sizes[]  = {sizeof(int), sizeof(double), sizeof(char[64])};
    size_t         counts[] = {8, 4, 2};
    cobalt_slab_t *slab     = cobalt_slab_create(sizes, counts, 3);
    TEST_ASSERT(slab != NULL);

    int    *a = (int *)cobalt_slab_alloc(slab, sizeof(int));
    double *b = (double *)cobalt_slab_alloc(slab, sizeof(double));
    char   *c = (char *)cobalt_slab_alloc(slab, 32);
    TEST_ASSERT(a != NULL && b != NULL && c != NULL);

    *a = 42;
    *b = 3.14;
    for (int i = 0; i < 32; i++) {
        c[i] = (char)(i + 'A');
    }

    TEST_ASSERT(*a == 42);
    TEST_ASSERT((*b > 3.0) && (*b < 4.0));

    cobalt_slab_free(slab, a);
    cobalt_slab_free(slab, b);
    cobalt_slab_free(slab, c);

    cobalt_slab_destroy(slab);
    printf("  Basic alloc/free: OK\n");
}

void test_slab_class_fit(void)
{
    printf("Testing size-class fitting...\n");

    size_t         sizes[]  = {sizeof(int), sizeof(double)};
    size_t         counts[] = {4, 4};
    cobalt_slab_t *slab     = cobalt_slab_create(sizes, counts, 2);
    TEST_ASSERT(slab != NULL);

    /* Request 2 bytes — should fit in sizeof(int) class */
    void *p = cobalt_slab_alloc(slab, 2);
    TEST_ASSERT(p != NULL);
    cobalt_slab_free(slab, p);

    /* Request exceeds all classes */
    TEST_ASSERT(cobalt_slab_alloc(slab, 1024) == NULL);

    cobalt_slab_destroy(slab);
    printf("  Size-class fitting: OK\n");
}

void test_slab_reuse(void)
{
    printf("Testing block reuse...\n");

    size_t         sizes[]  = {sizeof(int)};
    size_t         counts[] = {2};
    cobalt_slab_t *slab     = cobalt_slab_create(sizes, counts, 1);
    TEST_ASSERT(slab != NULL);

    int *a = (int *)cobalt_slab_alloc(slab, sizeof(int));
    int *b = (int *)cobalt_slab_alloc(slab, sizeof(int));
    TEST_ASSERT(a != NULL && b != NULL);

    cobalt_slab_free(slab, a);
    int *c = (int *)cobalt_slab_alloc(slab, sizeof(int));
    TEST_ASSERT(c != NULL);
    /* c should reuse a's block */
    *c = 99;
    TEST_ASSERT(*c == 99);

    cobalt_slab_free(slab, b);
    cobalt_slab_free(slab, c);
    cobalt_slab_destroy(slab);
    printf("  Block reuse: OK\n");
}

void test_slab_null_safe(void)
{
    printf("Testing null safety...\n");

    cobalt_slab_destroy(NULL);
    cobalt_slab_free(NULL, NULL);
    TEST_ASSERT(cobalt_slab_alloc(NULL, 8) == NULL);
    TEST_ASSERT(cobalt_slab_alloc(NULL, 0) == NULL);
    printf("  Null safety: OK\n");
}

void test_slab(void)
{
    printf("Testing slab allocator...\n");
    test_slab_basic();
    test_slab_class_fit();
    test_slab_reuse();
    test_slab_null_safe();
    printf("  Slab tests completed\n");
}
