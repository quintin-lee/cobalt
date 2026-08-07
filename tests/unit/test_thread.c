/**
 * @file test_thread.c
 * @brief Unit test for platform thread primitives.
 */

#include "cobalt/platform/thread.h"
#include "test_framework.h"
#include <stdio.h>

static int g_counter;
static cobalt_mutex_t *g_mutex;

static void *counter_thread(void *arg)
{
    (void)arg;
    for (int i = 0; i < 1000; i++) {
        cobalt_mutex_lock(g_mutex);
        g_counter++;
        cobalt_mutex_unlock(g_mutex);
    }
    return NULL;
}

void test_thread_mutex_counter(void)
{
    printf("Testing mutex with concurrent counter increment...\n");

    g_counter = 0;
    g_mutex   = cobalt_mutex_create();
    TEST_ASSERT(g_mutex != NULL);

    cobalt_thread_t threads[4];
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(cobalt_thread_create(counter_thread, NULL, &threads[i]) == 0);
    }
    for (int i = 0; i < 4; i++) {
        cobalt_thread_join(threads[i]);
    }

    TEST_ASSERT(g_counter == 4000);
    printf("  Counter = %d (expected 4000): OK\n", g_counter);

    cobalt_mutex_destroy(g_mutex);
    g_mutex = NULL;
}

static cobalt_cond_t *g_cond;
static int g_flag = 0;

static void *signal_thread(void *arg)
{
    (void)arg;
    cobalt_mutex_lock(g_mutex);
    g_flag = 1;
    cobalt_cond_signal(g_cond);
    cobalt_mutex_unlock(g_mutex);
    return NULL;
}

void test_thread_cond_signal(void)
{
    printf("Testing condition variable signal...\n");

    g_flag   = 0;
    g_mutex  = cobalt_mutex_create();
    g_cond   = cobalt_cond_create();
    TEST_ASSERT(g_mutex != NULL);
    TEST_ASSERT(g_cond != NULL);

    cobalt_thread_t thr;
    TEST_ASSERT(cobalt_thread_create(signal_thread, NULL, &thr) == 0);

    cobalt_mutex_lock(g_mutex);
    cobalt_cond_wait(g_cond, g_mutex);
    TEST_ASSERT(g_flag == 1);
    cobalt_mutex_unlock(g_mutex);

    cobalt_thread_join(thr);
    cobalt_cond_destroy(g_cond);
    cobalt_mutex_destroy(g_mutex);
}

void test_thread_timedwait(void)
{
    printf("Testing condition variable timed wait...\n");

    g_mutex  = cobalt_mutex_create();
    g_cond   = cobalt_cond_create();
    TEST_ASSERT(g_mutex != NULL);
    TEST_ASSERT(g_cond != NULL);

    cobalt_mutex_lock(g_mutex);
    int rc = cobalt_cond_timedwait(g_cond, g_mutex, 50); /* 50ms timeout */
    cobalt_mutex_unlock(g_mutex);
    TEST_ASSERT(rc == -1); /* timeout expected */
    printf("  Timed wait returns -1 on timeout: OK\n");

    cobalt_cond_destroy(g_cond);
    cobalt_mutex_destroy(g_mutex);
}

void test_thread_null_safety(void)
{
    printf("Testing null safety...\n");

    cobalt_mutex_destroy(NULL);
    cobalt_cond_destroy(NULL);
    cobalt_thread_yield();
    TEST_ASSERT(cobalt_thread_self() != 0);
    printf("  Null safety: OK\n");
}

void test_thread(void)
{
    printf("Testing thread primitives...\n");
    test_thread_mutex_counter();
    test_thread_cond_signal();
    test_thread_timedwait();
    test_thread_null_safety();
    printf("  Thread tests completed\n");
}
