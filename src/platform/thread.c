/**
 * @file thread.c
 * @brief Platform thread primitives implementation
 *
 * Provides mutex, condition variable, and thread abstractions.
 * Uses pthreads on Unix and Win32 API on Windows.
 */

#include "cobalt/platform/thread.h"
#include <stdlib.h>

#ifdef __APPLE__
#define COBALT_OS_DARWIN
#endif

#ifdef COBALT_OS_DARWIN
#include <pthread.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <pthread.h>
#include <time.h>
#endif

/* ========================================================================= */
/* Mutex                                                                      */
/* ========================================================================= */

struct cobalt_mutex {
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t m;
#endif
};

cobalt_mutex_t *cobalt_mutex_create(void)
{
    cobalt_mutex_t *m = (cobalt_mutex_t *)malloc(sizeof(cobalt_mutex_t));
    if (!m) {
        return NULL;
    }
#ifdef _WIN32
    InitializeCriticalSection(&m->cs);
#else
    if (pthread_mutex_init(&m->m, NULL) != 0) {
        free(m);
        return NULL;
    }
#endif
    return m;
}

void cobalt_mutex_destroy(cobalt_mutex_t *m)
{
    if (!m) {
        return;
    }
#ifdef _WIN32
    DeleteCriticalSection(&m->cs);
#else
    pthread_mutex_destroy(&m->m);
#endif
    free(m);
}

void cobalt_mutex_lock(cobalt_mutex_t *m)
{
    if (!m) {
        return;
    }
#ifdef _WIN32
    EnterCriticalSection(&m->cs);
#else
    pthread_mutex_lock(&m->m);
#endif
}

int cobalt_mutex_trylock(cobalt_mutex_t *m)
{
    if (!m) {
        return -1;
    }
#ifdef _WIN32
    /* Win32 has no try-enter; use a non-blocking approach */
    if (TryEnterCriticalSection(&m->cs)) {
        return 0;
    }
    return -1;
#else
    int rc = pthread_mutex_trylock(&m->m);
    return (rc == 0) ? 0 : -1;
#endif
}

void cobalt_mutex_unlock(cobalt_mutex_t *m)
{
    if (!m) {
        return;
    }
#ifdef _WIN32
    LeaveCriticalSection(&m->cs);
#else
    pthread_mutex_unlock(&m->m);
#endif
}

/* ========================================================================= */
/* Condition Variable                                                          */
/* ========================================================================= */

struct cobalt_cond {
#ifdef _WIN32
    HANDLE          event;
    cobalt_mutex_t *mutex;
#else
    pthread_cond_t c;
#endif
};

cobalt_cond_t *cobalt_cond_create(void)
{
    cobalt_cond_t *c = (cobalt_cond_t *)malloc(sizeof(cobalt_cond_t));
    if (!c) {
        return NULL;
    }
#ifdef _WIN32
    c->event = CreateEvent(NULL, TRUE, FALSE, NULL);
    c->mutex = NULL;
    if (!c->event) {
        free(c);
        return NULL;
    }
#else
    if (pthread_cond_init(&c->c, NULL) != 0) {
        free(c);
        return NULL;
    }
#endif
    return c;
}

void cobalt_cond_destroy(cobalt_cond_t *c)
{
    if (!c) {
        return;
    }
#ifdef _WIN32
    if (c->event) {
        CloseEvent(c->event);
    }
#else
    pthread_cond_destroy(&c->c);
#endif
    free(c);
}

void cobalt_cond_wait(cobalt_cond_t *c, cobalt_mutex_t *m)
{
    if (!c || !m) {
        return;
    }
#ifdef _WIN32
    /* Release mutex, wait on event */
    cobalt_mutex_unlock(m);
    WaitForSingleObject(c->event, INFINITE);
    cobalt_mutex_lock(m);
#else
    pthread_cond_wait(&c->c, &m->m);
#endif
}

void cobalt_cond_signal(cobalt_cond_t *c)
{
    if (!c) {
        return;
    }
#ifdef _WIN32
    SetEvent(c->event);
#else
    pthread_cond_signal(&c->c);
#endif
}

void cobalt_cond_broadcast(cobalt_cond_t *c)
{
    if (!c) {
        return;
    }
#ifdef _WIN32
    /* Win32 manual-reset event simulates broadcast */
    SetEvent(c->event);
    /* Reset after all waiters have acquired it */
    /* Note: this is a simplified broadcast for manual-reset events */
    ResetEvent(c->event);
#else
    pthread_cond_broadcast(&c->c);
#endif
}

int cobalt_cond_timedwait(cobalt_cond_t *c, cobalt_mutex_t *m, int64_t timeout_ms)
{
    if (!c || !m) {
        return -1;
    }
#ifdef _WIN32
    cobalt_mutex_unlock(m);
    DWORD dwMs   = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForSingleObject(c->event, dwMs);
    cobalt_mutex_lock(m);
    return (result == WAIT_TIMEOUT) ? -1 : 0;
#else
    if (timeout_ms < 0) {
        pthread_cond_wait(&c->c, &m->m);
        return 0;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)(timeout_ms / 1000);
    ts.tv_nsec += (long)((timeout_ms % 1000) * 1000000L);
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    int rc = pthread_cond_timedwait(&c->c, &m->m, &ts);
    return (rc == 0) ? 0 : -1;
#endif
}

/* ========================================================================= */
/* Thread                                                                      */
/* ========================================================================= */

#ifdef _WIN32
static unsigned __stdcall thread_proxy(void *arg)
{
    cobalt_thread_fn_t fn = (cobalt_thread_fn_t)arg;
    return (unsigned)fn(NULL);
}
#endif

int cobalt_thread_create(cobalt_thread_fn_t fn, void *arg, cobalt_thread_t *out_handle)
{
    if (!fn) {
        return -1;
    }
#ifdef _WIN32
    HANDLE h = (HANDLE)_beginthreadex(NULL, 0, thread_proxy, (void *)fn, 0, NULL);
    if (!h) {
        return -1;
    }
    if (out_handle) {
        *out_handle = (cobalt_thread_t)h;
    }
    return 0;
#else
    pthread_t *t = NULL;
    if (out_handle) {
        t = (pthread_t *)malloc(sizeof(pthread_t));
        if (!t) {
            return -1;
        }
    }
    int rc = pthread_create(t ? t : NULL, NULL, (void *(*)(void *))fn, arg);
    if (rc != 0) {
        free(t);
        return -1;
    }
    if (out_handle) {
        *out_handle = (cobalt_thread_t)t;
    }
    return 0;
#endif
}

void cobalt_thread_join(cobalt_thread_t handle)
{
    if (!handle) {
        return;
    }
#ifdef _WIN32
    WaitForSingleObject((HANDLE)handle, INFINITE);
    CloseHandle((HANDLE)handle);
#else
    pthread_join(*(pthread_t *)handle, NULL);
    free(handle);
#endif
}

void cobalt_thread_yield(void)
{
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

uintptr_t cobalt_thread_self(void)
{
#ifdef _WIN32
    return (uintptr_t)GetCurrentThreadId();
#else
    return (uintptr_t)pthread_self();
#endif
}
