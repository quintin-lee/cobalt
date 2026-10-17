/**
 * @file notifier.c
 * @brief Platform notification primitive implementation
 *
 * Provides cross-platform notification abstractions.
 * On Linux, uses eventfd for anonymous and named FIFO for IPC.
 * On Windows, uses Win32 auto-reset event objects.
 */

#include "cobalt/platform/notifier.h"
#include "cobalt/runtime/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/eventfd.h>
#endif
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#define COBALT_NOTIFIER_PATH_MAX 256

/**
 * @brief Opaque notifier structure
 */
struct cobalt_notifier {
#ifdef _WIN32
    HANDLE handle;
#else
    int  fd;
    int  write_fd;
    bool is_eventfd;
    bool is_creator;
    char path[COBALT_NOTIFIER_PATH_MAX];
#endif
};

#ifdef _WIN32

static int format_notifier_name(const char *name, char *buf, size_t buf_size)
{
    if (!name || !buf || buf_size == 0) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    const char *p = name;
    while (*p == '/') {
        p++;
    }
    if (*p == '\0') {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    if (strlen(p) >= buf_size) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    strncpy(buf, p, buf_size - 1);
    buf[buf_size - 1] = '\0';
    return COBALT_SUCCESS;
}

int cobalt_notifier_create(const char *name, cobalt_notifier_t **out_notif)
{
    if (!out_notif) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    *out_notif = NULL;

    char        win_name[COBALT_NOTIFIER_PATH_MAX];
    const char *event_name = NULL;
    if (name) {
        int rc = format_notifier_name(name, win_name, sizeof(win_name));
        if (rc != COBALT_SUCCESS) {
            cobalt_error_set(NULL, rc);
            return rc;
        }
        event_name = win_name;
    }

    HANDLE handle = CreateEventA(NULL, FALSE, FALSE, event_name);
    if (!handle) {
        cobalt_error_set(NULL, COBALT_ERROR_IO);
        return COBALT_ERROR_IO;
    }
    if (event_name && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(handle);
        cobalt_error_set(NULL, COBALT_ERROR_ALREADY_EXISTS);
        return COBALT_ERROR_ALREADY_EXISTS;
    }

    cobalt_notifier_t *notif = (cobalt_notifier_t *)malloc(sizeof(cobalt_notifier_t));
    if (!notif) {
        CloseHandle(handle);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }
    notif->handle = handle;
    *out_notif    = notif;

    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

int cobalt_notifier_open(const char *name, cobalt_notifier_t **out_notif)
{
    if (!name || !out_notif) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    *out_notif = NULL;

    char win_name[COBALT_NOTIFIER_PATH_MAX];
    int  rc = format_notifier_name(name, win_name, sizeof(win_name));
    if (rc != COBALT_SUCCESS) {
        cobalt_error_set(NULL, rc);
        return rc;
    }

    HANDLE handle = OpenEventA(EVENT_ALL_ACCESS, FALSE, win_name);
    if (!handle) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            cobalt_error_set(NULL, COBALT_ERROR_NOT_FOUND);
            return COBALT_ERROR_NOT_FOUND;
        }
        cobalt_error_set(NULL, COBALT_ERROR_IO);
        return COBALT_ERROR_IO;
    }

    cobalt_notifier_t *notif = (cobalt_notifier_t *)malloc(sizeof(cobalt_notifier_t));
    if (!notif) {
        CloseHandle(handle);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }
    notif->handle = handle;
    *out_notif    = notif;

    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

int cobalt_notifier_notify(cobalt_notifier_t *notif)
{
    if (!notif) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    if (!SetEvent(notif->handle)) {
        cobalt_error_set(NULL, COBALT_ERROR_IO);
        return COBALT_ERROR_IO;
    }

    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

int cobalt_notifier_wait(cobalt_notifier_t *notif, int timeout_ms)
{
    if (!notif || timeout_ms < -1) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    DWORD wait_res =
        WaitForSingleObject(notif->handle, timeout_ms == -1 ? INFINITE : (DWORD)timeout_ms);
    if (wait_res == WAIT_OBJECT_0) {
        cobalt_error_set(NULL, COBALT_SUCCESS);
        return COBALT_SUCCESS;
    } else if (wait_res == WAIT_TIMEOUT) {
        cobalt_error_set(NULL, COBALT_ERROR_TIMEOUT);
        return COBALT_ERROR_TIMEOUT;
    } else {
        cobalt_error_set(NULL, COBALT_ERROR_IO);
        return COBALT_ERROR_IO;
    }
}

int cobalt_notifier_get_fd(const cobalt_notifier_t *notif)
{
    (void)notif;
    return -1;
}

void cobalt_notifier_destroy(cobalt_notifier_t *notif)
{
    if (!notif) {
        return;
    }
    if (notif->handle) {
        CloseHandle(notif->handle);
    }
    free(notif);
}

#else /* POSIX / Linux */

static const char *get_fifo_base_dir(void)
{
    if (access("/dev/shm", W_OK | X_OK) == 0) {
        return "/dev/shm";
    }
    return "/tmp";
}

static int format_notifier_path(const char *name, char *buf, size_t buf_size)
{
    if (!name || !buf || buf_size == 0) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    size_t len = strlen(name);
    if (len == 0) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    if (strncmp(name, "/dev/shm/", 9) == 0 || strncmp(name, "/tmp/", 5) == 0) {
        if (len >= buf_size) {
            return COBALT_ERROR_INVALID_ARGUMENT;
        }
        strncpy(buf, name, buf_size - 1);
        buf[buf_size - 1] = '\0';
        return COBALT_SUCCESS;
    }

    const char *p = name;
    while (*p == '/') {
        p++;
    }
    if (*p == '\0') {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    if (strchr(p, '/') != NULL) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    const char *base    = get_fifo_base_dir();
    int         written = snprintf(buf, buf_size, "%s/%s", base, p);
    if (written < 0 || (size_t)written >= buf_size) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    return COBALT_SUCCESS;
}

int cobalt_notifier_create(const char *name, cobalt_notifier_t **out_notif)
{
    if (!out_notif) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    *out_notif = NULL;

    cobalt_notifier_t *notif = (cobalt_notifier_t *)malloc(sizeof(cobalt_notifier_t));
    if (!notif) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }
    memset(notif, 0, sizeof(*notif));

    if (name == NULL) {
#ifdef __linux__
        int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (fd < 0) {
            free(notif);
            cobalt_error_set(NULL, COBALT_ERROR_IO);
            return COBALT_ERROR_IO;
        }
        notif->fd         = fd;
        notif->write_fd   = fd;
        notif->is_eventfd = true;
        notif->is_creator = false;
        notif->path[0]    = '\0';
#else
        int fds[2];
        if (pipe(fds) != 0) {
            free(notif);
            cobalt_error_set(NULL, COBALT_ERROR_IO);
            return COBALT_ERROR_IO;
        }
        fcntl(fds[0], F_SETFD, FD_CLOEXEC);
        fcntl(fds[1], F_SETFD, FD_CLOEXEC);
        fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK);
        fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL, 0) | O_NONBLOCK);
        notif->fd         = fds[0];
        notif->write_fd   = fds[1];
        notif->is_eventfd = false;
        notif->is_creator = false;
        notif->path[0]    = '\0';
#endif
        *out_notif = notif;
        cobalt_error_set(NULL, COBALT_SUCCESS);
        return COBALT_SUCCESS;
    }

    char path[COBALT_NOTIFIER_PATH_MAX];
    int  rc = format_notifier_path(name, path, sizeof(path));
    if (rc != COBALT_SUCCESS) {
        free(notif);
        cobalt_error_set(NULL, rc);
        return rc;
    }

    if (mkfifo(path, 0666) != 0) {
        int err = COBALT_ERROR_IO;
        if (errno == EEXIST) {
            err = COBALT_ERROR_ALREADY_EXISTS;
        } else if (errno == EACCES) {
            err = COBALT_ERROR_PERMISSION_DENIED;
        }
        free(notif);
        cobalt_error_set(NULL, err);
        return err;
    }

    int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        unlink(path);
        free(notif);
        cobalt_error_set(NULL, COBALT_ERROR_IO);
        return COBALT_ERROR_IO;
    }

    notif->fd         = fd;
    notif->write_fd   = fd;
    notif->is_eventfd = false;
    notif->is_creator = true;
    strncpy(notif->path, path, sizeof(notif->path) - 1);
    notif->path[sizeof(notif->path) - 1] = '\0';

    *out_notif = notif;
    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

int cobalt_notifier_open(const char *name, cobalt_notifier_t **out_notif)
{
    if (!name || !out_notif) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    *out_notif = NULL;

    char path[COBALT_NOTIFIER_PATH_MAX];
    int  rc = format_notifier_path(name, path, sizeof(path));
    if (rc != COBALT_SUCCESS) {
        cobalt_error_set(NULL, rc);
        return rc;
    }

    int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        int err = COBALT_ERROR_IO;
        if (errno == ENOENT) {
            err = COBALT_ERROR_NOT_FOUND;
        } else if (errno == EACCES) {
            err = COBALT_ERROR_PERMISSION_DENIED;
        }
        cobalt_error_set(NULL, err);
        return err;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISFIFO(st.st_mode)) {
        close(fd);
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    cobalt_notifier_t *notif = (cobalt_notifier_t *)malloc(sizeof(cobalt_notifier_t));
    if (!notif) {
        close(fd);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }
    notif->fd         = fd;
    notif->write_fd   = fd;
    notif->is_eventfd = false;
    notif->is_creator = false;
    strncpy(notif->path, path, sizeof(notif->path) - 1);
    notif->path[sizeof(notif->path) - 1] = '\0';

    *out_notif = notif;
    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

int cobalt_notifier_notify(cobalt_notifier_t *notif)
{
    if (!notif) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    if (notif->is_eventfd) {
        uint64_t val = 1;
        ssize_t  ret;
        do {
            ret = write(notif->write_fd, &val, sizeof(val));
        } while (ret < 0 && errno == EINTR);

        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cobalt_error_set(NULL, COBALT_SUCCESS);
                return COBALT_SUCCESS;
            }
            cobalt_error_set(NULL, COBALT_ERROR_IO);
            return COBALT_ERROR_IO;
        }
    } else {
        char    byte = 1;
        ssize_t ret;
        do {
            ret = write(notif->write_fd, &byte, sizeof(byte));
        } while (ret < 0 && errno == EINTR);

        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cobalt_error_set(NULL, COBALT_SUCCESS);
                return COBALT_SUCCESS;
            }
            cobalt_error_set(NULL, COBALT_ERROR_IO);
            return COBALT_ERROR_IO;
        }
    }

    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

int cobalt_notifier_wait(cobalt_notifier_t *notif, int timeout_ms)
{
    if (!notif || timeout_ms < -1) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    struct pollfd pfd;
    pfd.fd      = notif->fd;
    pfd.events  = POLLIN;
    pfd.revents = 0;

    int ret;
    if (timeout_ms < 0) {
        do {
            ret = poll(&pfd, 1, -1);
        } while (ret < 0 && errno == EINTR);
    } else if (timeout_ms == 0) {
        ret = poll(&pfd, 1, 0);
    } else {
        struct timespec start, now;
        clock_gettime(CLOCK_MONOTONIC, &start);
        int remaining = timeout_ms;
        do {
            ret = poll(&pfd, 1, remaining);
            if (ret < 0 && errno == EINTR) {
                clock_gettime(CLOCK_MONOTONIC, &now);
                long elapsed = (long)(now.tv_sec - start.tv_sec) * 1000L +
                               (now.tv_nsec - start.tv_nsec) / 1000000L;
                if (elapsed >= timeout_ms) {
                    ret = 0;
                    break;
                }
                remaining = timeout_ms - (int)elapsed;
            }
        } while (ret < 0 && errno == EINTR);
    }

    if (ret < 0) {
        cobalt_error_set(NULL, COBALT_ERROR_IO);
        return COBALT_ERROR_IO;
    }
    if (ret == 0) {
        cobalt_error_set(NULL, COBALT_ERROR_TIMEOUT);
        return COBALT_ERROR_TIMEOUT;
    }

    if (pfd.revents & POLLIN) {
        if (notif->is_eventfd) {
            uint64_t val;
            ssize_t  n;
            do {
                n = read(notif->fd, &val, sizeof(val));
            } while (n < 0 && errno == EINTR);
        } else {
            char    buf[128];
            ssize_t n;
            do {
                n = read(notif->fd, buf, sizeof(buf));
            } while (n > 0 || (n < 0 && errno == EINTR));
        }
        cobalt_error_set(NULL, COBALT_SUCCESS);
        return COBALT_SUCCESS;
    }

    cobalt_error_set(NULL, COBALT_ERROR_IO);
    return COBALT_ERROR_IO;
}

int cobalt_notifier_get_fd(const cobalt_notifier_t *notif)
{
    if (!notif) {
        return -1;
    }
    return notif->fd;
}

void cobalt_notifier_destroy(cobalt_notifier_t *notif)
{
    if (!notif) {
        return;
    }
    if (notif->write_fd >= 0 && notif->write_fd != notif->fd) {
        close(notif->write_fd);
    }
    if (notif->fd >= 0) {
        close(notif->fd);
    }
    if (notif->is_creator && notif->path[0] != '\0') {
        unlink(notif->path);
    }
    free(notif);
}

#endif
