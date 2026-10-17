#ifndef COBALT_PLATFORM_NOTIFIER_H
#define COBALT_PLATFORM_NOTIFIER_H

/**
 * @file notifier.h
 * @brief Platform notification primitive
 * @details Provides cross-process and thread-to-thread notification mechanisms.
 *          On Linux uses eventfd for anonymous and named FIFO for IPC.
 *          On Windows uses Win32 auto-reset event objects.
 *
 * @defgroup PlatformNotifier Notification Primitive
 * @ingroup Platform
 * @{
 */

#include <stdbool.h>
#include <stddef.h>

typedef struct cobalt_notifier cobalt_notifier_t;

/**
 * @brief Create a new cross-process or anonymous notification primitive.
 * @param name Optional name for cross-process named notification. If NULL, creates an anonymous
 * notifier (e.g. eventfd or pipe).
 * @param out_notif Pointer to store the created notifier handle
 * @return 0 (COBALT_SUCCESS) on success, negative error code otherwise
 */
int cobalt_notifier_create(const char *name, cobalt_notifier_t **out_notif);

/**
 * @brief Open an existing named notification primitive.
 * @param name Name of the notification primitive (must not be NULL)
 * @param out_notif Pointer to store the opened notifier handle
 * @return 0 on success, negative error code otherwise
 */
int cobalt_notifier_open(const char *name, cobalt_notifier_t **out_notif);

/**
 * @brief Signal/notify the waiting peer(s).
 * @param notif Notifier handle
 * @return 0 on success, negative error code otherwise
 */
int cobalt_notifier_notify(cobalt_notifier_t *notif);

/**
 * @brief Wait for a notification with a timeout.
 * @param notif Notifier handle
 * @param timeout_ms Timeout in milliseconds (-1 for infinite, 0 for non-blocking poll)
 * @return 0 on success, COBALT_ERROR_TIMEOUT on timeout, negative error code otherwise
 */
int cobalt_notifier_wait(cobalt_notifier_t *notif, int timeout_ms);

/**
 * @brief Get the underlying pollable file descriptor (for epoll/poll/kqueue/eventloop).
 * @param notif Notifier handle
 * @return File descriptor >= 0 on POSIX, or -1 on Windows / unsupported.
 */
int cobalt_notifier_get_fd(const cobalt_notifier_t *notif);

/**
 * @brief Destroy and release the notifier handle and associated OS resources.
 * @param notif Notifier handle
 */
void cobalt_notifier_destroy(cobalt_notifier_t *notif);

/** @} */

#endif /* COBALT_PLATFORM_NOTIFIER_H */
