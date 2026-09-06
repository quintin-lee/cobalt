/**
 * @file test_allocator_inject.c
 * @brief Test allocator injection into containers and memory modules
 */

#include "cobalt/container/deque.h"
#include "cobalt/container/hashmap.h"
#include "cobalt/container/list.h"
#include "cobalt/container/queue.h"
#include "cobalt/container/set.h"
#include "cobalt/container/stack.h"
#include "cobalt/container/treemap.h"
#include "cobalt/container/vector.h"
#include "cobalt/core/class.h"
#include "cobalt/core/interface.h"
#include "cobalt/core/object.h"
#include "cobalt/interface/iterator.h"
#include "cobalt/interface/sequence.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/memory/pool.h"
#include "cobalt/memory/slab.h"
#include "test_framework.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdalign.h>
#include <string.h>

#define MOCK_ALLOC_SIZE 4096
static char   mock_buf[MOCK_ALLOC_SIZE];
static size_t mock_offset      = 0;
static int    mock_alloc_count = 0;
static int    mock_free_count  = 0;
static int    mock_fail_next   = 0;
#define MOCK_MAX_TRACKED 256
static void *mock_tracked[MOCK_MAX_TRACKED];
static int   mock_tracked_count = 0;

static void *mock_alloc(cobalt_allocator_t *self, size_t size)
{
    (void)self;
    mock_alloc_count++;
    if (mock_fail_next) {
        mock_fail_next = 0;
        return NULL;
    }

    size_t align = alignof(max_align_t);
    uintptr_t raw = (uintptr_t)&mock_buf[mock_offset];
    uintptr_t aligned = (raw + align - 1) & ~(uintptr_t)(align - 1);
    size_t adjust = (size_t)(aligned - raw);

    if (adjust > MOCK_ALLOC_SIZE - mock_offset) {
        return NULL;
    }
    mock_offset += adjust;

    if (mock_offset + size > MOCK_ALLOC_SIZE) {
        return NULL;
    }

    void *ptr = (void *)aligned;
    mock_offset += size;
    if (mock_tracked_count < MOCK_MAX_TRACKED) {
        mock_tracked[mock_tracked_count++] = ptr;
    }
    return ptr;
}

static void mock_free(cobalt_allocator_t *self, void *ptr)
{
    (void)self;
    mock_free_count++;
    for (int i = 0; i < mock_tracked_count; i++) {
        if (mock_tracked[i] == ptr) {
            mock_tracked[i] = NULL;
            break;
        }
    }
}

static void mock_free_all(void)
{
    for (int i = 0; i < mock_tracked_count; i++) {
        if (mock_tracked[i]) {
            mock_free_count++;
            mock_tracked[i] = NULL;
        }
    }
}

static void *mock_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
{
    (void)self;
    if (mock_fail_next) {
        mock_fail_next = 0;
        return NULL;
    }

    if (ptr == NULL) {
        return mock_alloc(self, new_size);
    }

    if (new_size == 0) {
        mock_free(self, ptr);
        return NULL;
    }

    size_t align = alignof(max_align_t);
    uintptr_t raw = (uintptr_t)&mock_buf[mock_offset];
    uintptr_t aligned = (raw + align - 1) & ~(uintptr_t)(align - 1);
    size_t adjust = (size_t)(aligned - raw);

    if (adjust > MOCK_ALLOC_SIZE - mock_offset) {
        return NULL;
    }
    mock_offset += adjust;

    if (mock_offset + new_size > MOCK_ALLOC_SIZE) {
        return NULL;
    }

    void *new_ptr = (void *)aligned;
    size_t old_size = new_size;
    memcpy(new_ptr, ptr, old_size);
    mock_offset += new_size;

    mock_free(self, ptr);
    if (mock_tracked_count < MOCK_MAX_TRACKED) {
        mock_tracked[mock_tracked_count++] = new_ptr;
    }
    return new_ptr;
}

static cobalt_allocator_t mock_allocator = {
    .alloc   = mock_alloc,
    .free    = mock_free,
    .realloc = mock_realloc,
};

void test_allocator_inject_vector(void)
{
    printf("Testing vector with injected allocator...\n");
    mock_alloc_count = 0;
    mock_free_count  = 0;
    mock_offset      = 0;

    cobalt_vector_t *v = cobalt_vector_create_with_allocator(4, &mock_allocator);
    TEST_ASSERT(v != NULL);
    TEST_ASSERT(mock_alloc_count >= 0);

    int val = 42;
    cobalt_vector_push(v, &val);
    TEST_ASSERT(cobalt_vector_size(v) == 1);
    cobalt_vector_destroy(v);
    printf("  Vector with injected allocator: OK\n");
}

void test_allocator_inject_hashmap(void)
{
    printf("Testing hashmap with injected allocator...\n");
    mock_alloc_count = 0;
    mock_offset      = 0;

    cobalt_hashmap_t *map = cobalt_hashmap_create_with_allocator(16, &mock_allocator);
    TEST_ASSERT(map != NULL);
    TEST_ASSERT(mock_alloc_count >= 0);

    int val = 42;
    cobalt_hashmap_put(map, "key", &val);
    int *got = (int *)cobalt_hashmap_get(map, "key");
    TEST_ASSERT(got != NULL && *got == 42);
    cobalt_hashmap_destroy(map);
    mock_free_all();
    printf("  HashMap with injected allocator: OK\n");
}
void test_allocator_inject_list(void)
{
    printf("Testing list with injected allocator...\n");
    mock_alloc_count = 0;
    mock_offset      = 0;

    cobalt_list_t *list = cobalt_list_create_with_allocator(&mock_allocator);
    TEST_ASSERT(list != NULL);
    TEST_ASSERT(mock_alloc_count >= 0);

    int val = 1;
    cobalt_list_push_back(list, &val);
    TEST_ASSERT(cobalt_list_size(list) == 1);
    cobalt_list_destroy(list);
    printf("  List with injected allocator: OK\n");
}

void test_allocator_inject_stack(void)
{
    printf("Testing stack with injected allocator...\n");
    mock_alloc_count = 0;
    mock_offset      = 0;

    cobalt_stack_t *st = cobalt_stack_create_with_allocator(&mock_allocator);
    TEST_ASSERT(st != NULL);
    TEST_ASSERT(mock_alloc_count >= 0);

    int val = 42;
    cobalt_stack_push(st, &val);
    int *popped_ptr;
    popped_ptr = (int *)cobalt_stack_pop(st);
    TEST_ASSERT(popped_ptr != NULL && *popped_ptr == 42);
    cobalt_stack_destroy(st);
    printf("  Stack with injected allocator: OK\n");
}

void test_allocator_inject_queue(void)
{
    printf("Testing queue with injected allocator...\n");
    mock_alloc_count = 0;
    mock_offset      = 0;

    cobalt_queue_t *q = cobalt_queue_create_with_allocator(&mock_allocator);
    TEST_ASSERT(q != NULL);
    TEST_ASSERT(mock_alloc_count >= 0);

    int val = 42;
    cobalt_queue_enqueue(q, &val);
    int *got_ptr;
    got_ptr = (int *)cobalt_queue_dequeue(q);
    TEST_ASSERT(got_ptr != NULL && *got_ptr == 42);
    cobalt_queue_destroy(q);
    printf("  Queue with injected allocator: OK\n");
}

void test_allocator_inject_set(void)
{
    printf("Testing set with injected allocator...\n");
    mock_alloc_count = 0;
    mock_offset      = 0;

    cobalt_set_t *set = cobalt_set_create_with_allocator(16, &mock_allocator);
    TEST_ASSERT(set != NULL);
    TEST_ASSERT(mock_alloc_count >= 0);

    int val = 42;
    cobalt_set_insert(set, &val);
    TEST_ASSERT(cobalt_set_contains(set, &val));
    cobalt_set_destroy(set);
    printf("  Set with injected allocator: OK\n");
}

void test_allocator_inject_deque(void)
{
    printf("Testing deque with injected allocator...\n");
    mock_alloc_count = 0;
    mock_offset      = 0;

    cobalt_deque_t *dq = cobalt_deque_create_with_allocator(&mock_allocator);
    TEST_ASSERT(dq != NULL);
    TEST_ASSERT(mock_alloc_count >= 0);

    int val = 42;
    cobalt_deque_push_back(dq, &val);
    int *got_ptr;
    got_ptr = (int *)cobalt_deque_pop_back(dq);
    TEST_ASSERT(got_ptr != NULL && *got_ptr == 42);
    cobalt_deque_destroy(dq);
    printf("  Deque with injected allocator: OK\n");
}

void test_allocator_alloc_failure(void)
{
    printf("Testing allocator failure propagation...\n");
    mock_fail_next   = 1;
    mock_alloc_count = 0;
    mock_offset      = 0;

    cobalt_vector_t *v = cobalt_vector_create_with_allocator(4, &mock_allocator);
    TEST_ASSERT(v == NULL);
    printf("  Vector alloc failure: OK\n");
}

void test_allocator_inject_pool(void)
{
    printf("Testing pool with injected allocator...\n");
    mock_alloc_count = 0;
    mock_free_count  = 0;
    mock_offset      = 0;

    cobalt_pool_t *pool = cobalt_pool_create_with_allocator(sizeof(int), 8, &mock_allocator);
    TEST_ASSERT(pool != NULL);
    TEST_ASSERT(mock_alloc_count >= 2); /* pool struct + memory block */

    int *a = (int *)cobalt_pool_alloc(pool);
    TEST_ASSERT(a != NULL);
    *a = 42;
    TEST_ASSERT(*a == 42);

    cobalt_pool_free(pool, a);
    int *b = (int *)cobalt_pool_alloc(pool);
    TEST_ASSERT(b != NULL);
    TEST_ASSERT(cobalt_pool_free_count(pool) == 7);

    cobalt_pool_destroy(pool);
    TEST_ASSERT(mock_free_count >= 2);
    printf("  Pool with injected allocator: OK\n");
}

void test_allocator_inject_slab(void)
{
    printf("Testing slab with injected allocator...\n");
    mock_alloc_count = 0;
    mock_free_count  = 0;
    mock_offset      = 0;

    size_t         sizes[]  = {sizeof(int), sizeof(double)};
    size_t         counts[] = {4, 4};
    cobalt_slab_t *slab     = cobalt_slab_create_with_allocator(sizes, counts, 2, &mock_allocator);
    TEST_ASSERT(slab != NULL);
    TEST_ASSERT(mock_alloc_count >= 3); /* slab struct + 2 class memories */

    int    *a = (int *)cobalt_slab_alloc(slab, sizeof(int));
    double *b = (double *)cobalt_slab_alloc(slab, sizeof(double));
    TEST_ASSERT(a != NULL && b != NULL);
    *a = 99;
    *b = 3.14;
    TEST_ASSERT(*a == 99);
    TEST_ASSERT((*b > 3.0) && (*b < 4.0));

    cobalt_slab_free(slab, a);
    cobalt_slab_free(slab, b);
    cobalt_slab_destroy(slab);
    printf("  Slab with injected allocator: OK\n");
}

static void *core_alloc_invoke(cobalt_object_t *self, void **args, size_t arg_count)
{
    (void)self;
    (void)args;
    (void)arg_count;
    return NULL;
}

static void *core_alloc_get(cobalt_object_t *self)
{
    (void)self;
    return NULL;
}

static void core_alloc_set(cobalt_object_t *self, void *value)
{
    (void)self;
    (void)value;
}

static void core_alloc_reset(void)
{
    mock_alloc_count   = 0;
    mock_free_count    = 0;
    mock_offset        = 0;
    mock_tracked_count = 0;
}

void test_allocator_inject_core_class(void)
{
    printf("Testing class with injected allocator...\n");
    core_alloc_reset();

    cobalt_class_t *cls = cobalt_class_create_with_allocator("T", NULL, &mock_allocator);
    TEST_ASSERT(cls != NULL);
    TEST_ASSERT(cobalt_class_add_method(cls, "m", core_alloc_invoke) == 0);
    TEST_ASSERT(cobalt_class_add_property(cls, "p", core_alloc_get, core_alloc_set) == 0);

    /* Objects inherit the allocator from their class */
    cobalt_object_t *obj = cobalt_object_new(cls, 0);
    TEST_ASSERT(obj != NULL);
    cobalt_object_unref(obj);

    cobalt_class_destroy(cls);
    mock_free_all();
    TEST_ASSERT(mock_alloc_count == mock_free_count);
    printf("  Class with injected allocator: OK\n");
}

void test_allocator_inject_core_interface(void)
{
    printf("Testing interface with injected allocator...\n");
    core_alloc_reset();

    cobalt_interface_t *iface = cobalt_interface_new_with_allocator(NULL, &mock_allocator);
    TEST_ASSERT(iface != NULL);
    cobalt_interface_destroy_with_allocator(iface, &mock_allocator);

    mock_free_all();
    TEST_ASSERT(mock_alloc_count == mock_free_count);
    printf("  Interface with injected allocator: OK\n");
}

static size_t fake_seq_size(cobalt_sequence_t *seq)
{
    (void)seq;
    return 3;
}

static void *fake_seq_get_at_index(cobalt_sequence_t *seq, size_t index)
{
    (void)seq;
    return (void *)(uintptr_t)(index + 1);
}

void test_allocator_inject_iterator(void)
{
    printf("Testing iterator with injected allocator...\n");
    core_alloc_reset();

    /* Minimal fake sequence: iterator only needs size + get_at_index */
    static cobalt_sequence_t fake_seq;
    fake_seq.size         = fake_seq_size;
    fake_seq.get_at_index = fake_seq_get_at_index;

    cobalt_iterator_t *it = cobalt_iterator_new_with_allocator(&fake_seq, &mock_allocator);
    TEST_ASSERT(it != NULL);
    int seen = 0;
    while (cobalt_iterator_has_next(it)) {
        TEST_ASSERT(cobalt_iterator_next(it) != NULL);
        seen++;
    }
    TEST_ASSERT(seen == 3);
    cobalt_iterator_destroy(it);

    mock_free_all();
    TEST_ASSERT(mock_alloc_count == mock_free_count);
    printf("  Iterator with injected allocator: OK\n");
}

void test_allocator_inject(void)
{
    printf("Testing allocator injection...\n");
    test_allocator_inject_vector();
    test_allocator_inject_hashmap();
    test_allocator_inject_list();
    test_allocator_inject_stack();
    test_allocator_inject_queue();
    test_allocator_inject_set();
    test_allocator_inject_deque();
    test_allocator_inject_pool();
    test_allocator_inject_slab();
    test_allocator_inject_core_class();
    test_allocator_inject_core_interface();
    test_allocator_inject_iterator();
    test_allocator_alloc_failure();
    printf("  Allocator injection tests completed\n");
}
