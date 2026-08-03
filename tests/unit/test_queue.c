/**
 * @file test_queue.c
 * @Unit test for queue container.
 */

#include "cobalt/container/queue.h"
#include "test_framework.h"
#include <stdio.h>

void test_queue_basic(void)
{
    printf("Testing queue basic operations...\n");

    cobalt_queue_t *queue = cobalt_queue_create();
    if (!queue) {
        fprintf(stderr, "ERROR: Failed to create queue\n");
        return;
    }

    /* Test empty */
    if (cobalt_queue_is_empty(queue)) {
        printf("  Queue is empty: OK\n");
    }

    /* Enqueue items */
    int a = 1, b = 2, c = 3;
    cobalt_queue_enqueue(queue, &a);
    cobalt_queue_enqueue(queue, &b);
    cobalt_queue_enqueue(queue, &c);

    if (cobalt_queue_size(queue) == 3) {
        printf("  Queue size after 3 enqueues: 3 OK\n");
    }

    /* Peek */
    int *front = (int *)cobalt_queue_peek(queue);
    if (front && *front == 1) {
        printf("  Peek returns 1: OK\n");
    }

    /* Dequeue - FIFO order */
    int *val = (int *)cobalt_queue_dequeue(queue);
    if (val && *val == 1) {
        printf("  Dequeue returns 1: OK\n");
    }

    val = (int *)cobalt_queue_dequeue(queue);
    if (val && *val == 2) {
        printf("  Dequeue returns 2: OK\n");
    }

    val = (int *)cobalt_queue_dequeue(queue);
    if (val && *val == 3) {
        printf("  Dequeue returns 3: OK\n");
    }

    if (cobalt_queue_is_empty(queue)) {
        printf("  Queue is empty after dequeues: OK\n");
    }

    cobalt_queue_destroy(queue);
    printf("  Queue tests completed\n");
}

void test_queue_edge_cases(void)
{
    printf("Testing queue edge cases...\n");

    /* Dequeue from empty queue */
    cobalt_queue_t *queue = cobalt_queue_create();
    TEST_ASSERT(queue != NULL);
    TEST_ASSERT(cobalt_queue_peek(queue) == NULL);
    TEST_ASSERT(cobalt_queue_dequeue(queue) == NULL);
    TEST_ASSERT(cobalt_queue_is_empty(queue));
    TEST_ASSERT(cobalt_queue_size(queue) == 0);

    /* Enqueue to NULL queue */
    TEST_ASSERT(cobalt_queue_enqueue(NULL, NULL) == -1);

    /* Destroy NULL queue */
    cobalt_queue_destroy(NULL); /* should not crash */

    cobalt_queue_destroy(queue);
    printf("  Queue edge cases OK\n");
}

void test_queue(void)
{
    printf("Testing queue...\n");
    test_queue_basic();
    test_queue_edge_cases();
}
