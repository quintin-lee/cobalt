#ifndef EVENTLOOP_H
#define EVENTLOOP_H

/**
 * @file eventloop.h
 * @brief Event-driven I/O loop module
 * @details Provides a single-threaded non-blocking event loop mechanism, supporting timers and
 * listening to file descriptor (FD) events.
 */

#include <cobalt/memory/allocator.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup EventLoop_Module Event Loop Module
 * @{
 */

/* Use different type aliases to prevent writing in the wrong order when passing parameters */
typedef int      cobalt_fd_t;          /**< File descriptor type */
typedef short    cobalt_events_t;      /**< Event type mask (e.g., POLLIN, POLLOUT, etc.) */
typedef uint64_t cobalt_timeout_ms_t;  /**< Timeout time (milliseconds) */
typedef uint64_t cobalt_interval_ms_t; /**< Timer interval time (milliseconds) */

/**
 * @brief File descriptor event handling callback function
 * @param file_desc File descriptor that triggered the event
 * @param events Mask of the triggered events
 * @param user_data User data passed during registration
 */
typedef void (*fd_handler_t)(cobalt_fd_t file_desc, cobalt_events_t events, void *user_data);

/**
 * @brief Timer event handling callback function
 * @param timer_id Triggered timer ID
 * @param user_data User data passed during registration
 */
typedef void (*timer_handler_t)(uint64_t timer_id, void *user_data);

/**
 * @brief Opaque structure for event loop context
 */
typedef struct cobalt_eventloop cobalt_eventloop_t;

/**
 * @brief Create a new event loop
 * @return Returns a pointer to the event loop object on success, NULL on failure
 */
cobalt_eventloop_t *cobalt_eventloop_create(void);
cobalt_eventloop_t *cobalt_eventloop_create_with_allocator(cobalt_allocator_t *alloc);

/**
 * @brief Destroy and free the event loop and its associated resources
 * @param loop Event loop object to destroy
 */
void cobalt_eventloop_destroy(cobalt_eventloop_t *loop);

/**
 * @brief Register a file descriptor event listener
 * @param loop Event loop object
 * @param fd File descriptor to listen to
 * @param events Event type to listen for
 * @param callback Callback function called when the event occurs
 * @param user_data User data to pass to the callback function
 * @return Returns 0 on success, -1 on failure
 */
int cobalt_eventloop_add_fd(cobalt_eventloop_t *loop,
                            cobalt_fd_t         fd,
                            cobalt_events_t     events,
                            fd_handler_t        callback,
                            void               *user_data);

/**
 * @brief Modify the listening events of an already registered file descriptor
 * @param loop Event loop object
 * @param fd Already registered file descriptor
 * @param events New listening event type
 * @param callback New callback function
 * @param user_data New user data to pass to the callback function
 * @return Returns 0 on success, -1 on failure
 */
int cobalt_eventloop_mod_fd(cobalt_eventloop_t *loop,
                            cobalt_fd_t         fd,
                            cobalt_events_t     events,
                            fd_handler_t        callback,
                            void               *user_data);

/**
 * @brief Remove a file descriptor event listener
 * @param loop Event loop object
 * @param fd File descriptor to remove
 * @return Returns 0 on success, -1 on failure
 */
int cobalt_eventloop_del_fd(cobalt_eventloop_t *loop, cobalt_fd_t fd);

/**
 * @brief Add a timer
 * @param loop Event loop object
 * @param timeout_ms Delay time for the first trigger (milliseconds)
 * @param interval_ms Interval time for periodic triggers (milliseconds, 0 means no loop)
 * @param callback Callback function called when the timer triggers
 * @param user_data User data to pass to the callback function
 * @return Returns a timer ID greater than 0 on success, 0 on failure
 */
uint64_t cobalt_eventloop_add_timer(cobalt_eventloop_t  *loop,
                                    cobalt_timeout_ms_t  timeout_ms,
                                    cobalt_interval_ms_t interval_ms,
                                    timer_handler_t      callback,
                                    void                *user_data);

/**
 * @brief Remove a specified timer
 * @param loop Event loop object
 * @param timer_id ID of the timer to remove
 * @return Returns 0 on success, -1 on failure
 */
int cobalt_eventloop_del_timer(cobalt_eventloop_t *loop, uint64_t timer_id);

/**
 * @brief Run the event loop (blocking)
 * @details Will block the current thread, continuously handling ready events, until
 * cobalt_eventloop_stop is called.
 * @param loop Event loop object to run
 */
void cobalt_eventloop_run(cobalt_eventloop_t *loop);

/**
 * @brief Stop a running event loop
 * @param loop Event loop object to stop
 */
void cobalt_eventloop_stop(cobalt_eventloop_t *loop);

/**
 * @brief Execute a single iteration of the event loop (non-blocking or short-time blocking)
 * @details Checks and processes currently ready timer and FD events, suitable for integration into
 * other loops.
 * @param loop Event loop object
 * @return Returns 0 on success, -1 on failure
 */
int cobalt_eventloop_iteration(cobalt_eventloop_t *loop);

/**
 * @brief Register a signal handler that delivers events through the event loop
 * @details Installs a signal handler using a ring buffer. The callback is invoked
 *          from cobalt_eventloop_iteration with fd set to the signum.
 * @param loop Event loop object (used for API consistency; signal handlers are global)
 * @param signum Signal number to catch (e.g., SIGINT=2, SIGTERM=15)
 * @param callback Callback invoked when the signal is received
 * @param user_data User data passed to the callback
 * @return Returns 0 on success, -1 on failure
 * @note Registering the same signal twice returns -1.
 * @note Maximum COBALT_MAX_SIGNAL_HANDLERS (32) concurrent signal handlers.
 */
int cobalt_eventloop_add_signal(cobalt_eventloop_t *loop,
                                int                 signum,
                                fd_handler_t        callback,
                                void               *user_data);

/**
 * @brief Register a close/cleanup callback invoked on cobalt_eventloop_destroy
 * @param loop Event loop object
 * @param callback Callback invoked with fd=-1, events=0 during destroy
 * @param user_data User data passed to the callback
 * @return Returns 0 on success, -1 on failure
 */
int cobalt_eventloop_add_close_callback(cobalt_eventloop_t *loop,
                                        fd_handler_t        callback,
                                        void               *user_data);

/**
 * @brief Create a UNIX domain socket server
 * @param path   Unix socket path (e.g. "/tmp/cobalt_test.sock")
 * @param sock_out Pointer to receive the new socket FD
 * @return Returns 0 on success, -1 on failure
 * @note The socket is automatically set to listen mode. The caller must
 *       register it with cobalt_eventloop_add_fd() to handle incoming
 *       connections. The socket path is unlinked on bind; stale sockets
 *       are removed automatically.
 */
int cobalt_eventloop_create_unix_server(const char *path, cobalt_fd_t *sock_out);

/**
 * @brief Accept a connection on a UNIX domain socket and register it with the event loop
 * @param loop     Event loop to register the connection on
 * @param listen_fd Listening socket FD (must already be registered with the loop)
 * @param events   Event mask to watch on the accepted connection (e.g. POLLIN)
 * @param callback Callback invoked when events occur on the new connection
 * @param user_data User data passed to the callback
 * @return Returns 0 on success, -1 on failure (e.g. EAGAIN / EWOULDBLOCK)
 * @note Returns -1 immediately if no connection is pending (non-blocking accept).
 */
int cobalt_eventloop_accept(cobalt_eventloop_t *loop,
                            cobalt_fd_t         listen_fd,
                            cobalt_events_t     events,
                            fd_handler_t        callback,
                            void               *user_data);

/** @} */

#endif /* EVENTLOOP_H */
