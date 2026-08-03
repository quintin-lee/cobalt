#include "cobalt/container/deque.h"
#include "test_framework.h"
#include <stdio.h>

void test_deque_basic(void)
{
    printf("Testing deque basic operations...\n");

    cobalt_deque_t *dq = cobalt_deque_create();
    TEST_ASSERT(dq != NULL);
    TEST_ASSERT(cobalt_deque_is_empty(dq));
    TEST_ASSERT(cobalt_deque_size(dq) == 0);

    int a = 1, b = 2, c = 3;

    TEST_ASSERT(cobalt_deque_push_back(dq, &a) == 0);
    TEST_ASSERT(cobalt_deque_push_back(dq, &b) == 0);
    TEST_ASSERT(cobalt_deque_push_back(dq, &c) == 0);
    TEST_ASSERT(cobalt_deque_size(dq) == 3);
    printf("  Push back: OK\n");

    TEST_ASSERT(cobalt_deque_push_front(dq, &a) == 0);
    TEST_ASSERT(cobalt_deque_size(dq) == 4);
    printf("  Push front: OK\n");

    void *item = cobalt_deque_pop_front(dq);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 1);
    printf("  Pop front: OK\n");

    item = cobalt_deque_pop_back(dq);
    TEST_ASSERT(item != NULL);
    TEST_ASSERT(*(int *)item == 3);
    printf("  Pop back: OK\n");

    TEST_ASSERT(cobalt_deque_size(dq) == 2);

    cobalt_deque_destroy(dq);
    printf("  Deque basic test passed\n");
}

void test_deque_peek(void)
{
    printf("Testing deque peek operations...\n");

    cobalt_deque_t *dq = cobalt_deque_create();
    TEST_ASSERT(dq != NULL);

    int a = 10, b = 20;
    cobalt_deque_push_back(dq, &a);
    cobalt_deque_push_back(dq, &b);

    void *front = cobalt_deque_peek_front(dq);
    TEST_ASSERT(front != NULL);
    TEST_ASSERT(*(int *)front == 10);

    void *back = cobalt_deque_peek_back(dq);
    TEST_ASSERT(back != NULL);
    TEST_ASSERT(*(int *)back == 20);

    TEST_ASSERT(cobalt_deque_size(dq) == 2);

    cobalt_deque_destroy(dq);
    printf("  Peek operations: OK\n");
}

void test_deque_empty(void)
{
    printf("Testing deque empty operations...\n");

    cobalt_deque_t *dq = cobalt_deque_create();
    TEST_ASSERT(dq != NULL);

    TEST_ASSERT(cobalt_deque_pop_front(dq) == NULL);
    TEST_ASSERT(cobalt_deque_pop_back(dq) == NULL);
    TEST_ASSERT(cobalt_deque_peek_front(dq) == NULL);
    TEST_ASSERT(cobalt_deque_peek_back(dq) == NULL);

    cobalt_deque_destroy(dq);

    TEST_ASSERT(cobalt_deque_pop_front(NULL) == NULL);
    TEST_ASSERT(cobalt_deque_pop_back(NULL) == NULL);
    cobalt_deque_destroy(NULL);
    printf("  Empty and NULL safety: OK\n");
}

void test_deque(void)
{
    printf("Testing deque...\n");
    test_deque_basic();
    test_deque_peek();
    test_deque_empty();
    printf("  Deque tests completed\n");
}
