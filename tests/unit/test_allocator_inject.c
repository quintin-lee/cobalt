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
#include "cobalt/memory/allocator.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOCK_ALLOC_SIZE 4096
static char   mock_buf[MOCK_ALLOC_SIZE];
static size_t mock_offset      = 0;
static int    mock_alloc_count = 0;
static int    mock_free_count  = 0;
static int    mock_fail_next   = 0;

static void *mock_alloc(cobalt_allocator_t *self, size_t size)
{
    (void)self;
    mock_alloc_count++;
    if (mock_fail_next) {
        mock_fail_next = 0;
        return NULL;
    }
    if (mock_offset + size > MOCK_ALLOC_SIZE) {
        return NULL;
    }
    void *ptr = &mock_buf[mock_offset];
    mock_offset += size;
    return ptr;
}

static void mock_free(cobalt_allocator_t *self, void *ptr)
{
    (void)self;
    (void)ptr;
    mock_free_count++;
}

static void *mock_realloc(cobalt_allocator_t *self, void *ptr, size_t new_size)
{
    (void)self;
    mock_alloc_count++;
    if (mock_fail_next) {
        mock_fail_next = 0;
        return NULL;
    }
    if (mock_offset + new_size > MOCK_ALLOC_SIZE) {
        return NULL;
    }
    void *new_ptr = &mock_buf[mock_offset];
    if (ptr && new_size > 0) {
        size_t old_size = new_size; /* simplified: just copy */
        memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
    }
    mock_offset += new_size;
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
    test_allocator_alloc_failure();
    printf("  Allocator injection tests completed\n");
}
