#ifndef PLATFORM_THREAD_H
#define PLATFORM_THREAD_H

/**
 * @file thread.h
 * @brief Platform thread primitives (mutex, condition variable)
 * @details Provides cross-platform mutex and condition variable abstractions.
 *          On Unix/Linux/macOS uses pthreads; on Windows uses Win32 synchronization API.
 *
 * @defgroup PlatformThread Thread Primitives
 * @ingroup Platform
 * @{
 */

#include <stdint.h>

/**
 * @brief Opaque mutex type
 */
typedef struct cobalt_mutex cobalt_mutex_t;

/**
 * @brief Opaque condition variable type
 */
typedef struct cobalt_cond cobalt_cond_t;

/**
 * @brief Thread handle type
 */
typedef void *cobalt_thread_t;

/**
 * @brief Thread function signature
 * @param arg User-supplied argument passed to the thread
 */
typedef void *(*cobalt_thread_fn_t)(void *arg);

/* -------------------------------------------------------------------------- */
/* Mutex                                                                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create and initialize a mutex
 * @return Newly created mutex, or NULL on failure
 */
cobalt_mutex_t *cobalt_mutex_create(void);

/**
 * @brief Destroy a mutex and release associated resources
 * @param mutex Mutex to destroy. No-op if NULL.
 */
void cobalt_mutex_destroy(cobalt_mutex_t *mutex);

/**
 * @brief Lock the mutex (blocking)
 * @param mutex Mutex to lock
 */
void cobalt_mutex_lock(cobalt_mutex_t *mutex);

/**
 * @brief Try to lock the mutex (non-blocking)
 * @param mutex Mutex to try-lock
 * @return 0 on success, -1 if the mutex is already locked
 */
int cobalt_mutex_trylock(cobalt_mutex_t *mutex);

/**
 * @brief Unlock the mutex
 * @param mutex Mutex to unlock
 */
void cobalt_mutex_unlock(cobalt_mutex_t *mutex);

/* -------------------------------------------------------------------------- */
/* Condition Variable                                                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create and initialize a condition variable
 * @return Newly created condition variable, or NULL on failure
 */
cobalt_cond_t *cobalt_cond_create(void);

/**
 * @brief Destroy a condition variable and release associated resources
 * @param cond Condition variable to destroy. No-op if NULL.
 */
void cobalt_cond_destroy(cobalt_cond_t *cond);

/**
 * @brief Wait on the condition variable (releases mutex, blocks until signaled)
 * @param cond Condition variable
 * @param mutex  Associated mutex (must be locked by caller)
 */
void cobalt_cond_wait(cobalt_cond_t *cond, cobalt_mutex_t *mutex);

/**
 * @brief Signal one waiting thread
 * @param cond Condition variable
 */
void cobalt_cond_signal(cobalt_cond_t *cond);

/**
 * @brief Wake all waiting threads
 * @param cond Condition variable
 */
void cobalt_cond_broadcast(cobalt_cond_t *cond);

/**
 * @brief Wait with timeout (milliseconds)
 * @param cond     Condition variable
 * @param mutex    Associated mutex
 * @param timeout_ms Timeout in milliseconds; 0 = poll, -1 = wait indefinitely
 * @return 0 on signalled, -1 on timeout
 */
int cobalt_cond_timedwait(cobalt_cond_t *cond, cobalt_mutex_t *mutex, int64_t timeout_ms);

/* -------------------------------------------------------------------------- */
/* Thread                                                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create and start a new thread
 * @param fn     Thread entry point
 * @param arg    Argument passed to the thread function
 * @param out_handle  Output parameter for the thread handle (may be NULL)
 * @return 0 on success, -1 on failure
 */
int cobalt_thread_create(cobalt_thread_fn_t fn, void *arg, cobalt_thread_t *out_handle);

/**
 * @brief Wait for a thread to finish and release its resources
 * @param handle Thread handle returned by cobalt_thread_create
 */
void cobalt_thread_join(cobalt_thread_t handle);

/**
 * @brief Yield the current thread's time slice
 */
void cobalt_thread_yield(void);

/**
 * @brief Get the current thread's ID (for debugging)
 * @return Opaque thread ID value
 */
uintptr_t cobalt_thread_self(void);

/** @} */

#endif /* PLATFORM_THREAD_H */
