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
#include <process.h>
#else
#include <errno.h>
#include <pthread.h>
#include <time.h>
#endif

/* ========================================================================= */
/* Mutex                                                                      */
/* ========================================================================= */

/**
 * @brief Opaque mutex structure
 * @details Wraps CRITICAL_SECTION on Windows, pthread_mutex_t otherwise.
 */
struct cobalt_mutex {
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t m;
#endif
};

/**
 * @brief Create and initialize a mutex
 * @return Pointer to the created mutex, or NULL on allocation or init failure
 */
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

/**
 * @brief Destroy a mutex and release its resources
 * @param m Pointer to the mutex; if NULL, no action is taken
 */
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

/**
 * @brief Lock a mutex, blocking until acquired
 * @param m Pointer to the mutex; if NULL, no action is taken
 */
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

/**
 * @brief Attempt to lock a mutex without blocking
 * @param m Pointer to the mutex; if NULL, -1 is returned
 * @return 0 on success, -1 if the mutex is already locked or unavailable
 */
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

/**
 * @brief Unlock a mutex
 * @param m Pointer to the mutex; if NULL, no action is taken
 */
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

/**
 * @brief Opaque condition variable structure
 * @details Wraps an event plus mutex on Windows, pthread_cond_t otherwise.
 */
struct cobalt_cond {
#ifdef _WIN32
    HANDLE          event;
    cobalt_mutex_t *mutex;
#else
    pthread_cond_t c;
#endif
};

/**
 * @brief Create and initialize a condition variable
 * @return Pointer to the created condition variable, or NULL on failure
 */
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

/**
 * @brief Destroy a condition variable and release its resources
 * @param c Pointer to the condition variable; if NULL, no action is taken
 */
void cobalt_cond_destroy(cobalt_cond_t *c)
{
    if (!c) {
        return;
    }
#ifdef _WIN32
    if (c->event) {
        CloseHandle(c->event);
    }
#else
    pthread_cond_destroy(&c->c);
#endif
    free(c);
}

/**
 * @brief Wait on a condition variable, releasing the mutex while blocked
 * @details The mutex must be locked by the caller; it is re-acquired before returning
 * @param c Pointer to the condition variable
 * @param m Pointer to the associated mutex
 */
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

/**
 * @brief Wake one thread waiting on a condition variable
 * @param c Pointer to the condition variable; if NULL, no action is taken
 */
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

/**
 * @brief Wake all threads waiting on a condition variable
 * @param c Pointer to the condition variable; if NULL, no action is taken
 */
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

/**
 * @brief Wait on a condition variable with a timeout
 * @details A negative timeout waits indefinitely; otherwise waits at most timeout_ms
 * @param c Pointer to the condition variable
 * @param m Pointer to the associated mutex
 * @param timeout_ms Maximum wait time in milliseconds, or negative for no timeout
 * @return 0 if signaled, -1 on timeout, invalid arguments, or wait failure
 */
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
/**
 * @brief Win32 thread entry trampoline (adapts cobalt_thread_fn_t to _beginthreadex)
 * @param arg Thread function pointer passed through _beginthreadex
 * @return Return value of the wrapped thread function
 */
static unsigned __stdcall thread_proxy(void *arg)
{
    cobalt_thread_fn_t fn = (cobalt_thread_fn_t)arg;
    return (unsigned)fn(NULL);
}
#endif

/**
 * @brief Create a new thread running the given function
 * @details If out_handle is NULL the thread is detached; otherwise the caller must join it.
 * On allocation failure after thread creation the new thread is detached to avoid leaking it.
 * @param fn Thread entry function (must not be NULL)
 * @param arg Argument passed to the thread function
 * @param out_handle Optional output receiving the thread handle (may be NULL)
 * @return 0 on success, -1 on failure
 */
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
    pthread_t tmp_tid;
    int       rc = pthread_create(&tmp_tid, NULL, (void *(*)(void *))fn, arg);
    if (rc != 0) {
        return -1;
    }
    if (out_handle) {
        pthread_t *t = (pthread_t *)malloc(sizeof(pthread_t));
        if (!t) {
            pthread_detach(tmp_tid);
            return -1;
        }
        *t          = tmp_tid;
        *out_handle = (cobalt_thread_t)t;
    } else {
        pthread_detach(tmp_tid);
    }
    return 0;
#endif
}

/**
 * @brief Wait for a thread to finish and release its handle
 * @param handle Thread handle from cobalt_thread_create; if NULL, no action is taken
 */
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

/**
 * @brief Yield the processor, allowing another thread to run
 */
void cobalt_thread_yield(void)
{
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

/**
 * @brief Get an identifier for the calling thread
 * @return Opaque numeric thread identifier
 */
uintptr_t cobalt_thread_self(void)
{
#ifdef _WIN32
    return (uintptr_t)GetCurrentThreadId();
#else
    return (uintptr_t)pthread_self();
#endif
}
