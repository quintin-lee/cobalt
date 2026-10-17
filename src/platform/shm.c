/**
 * @file shm.c
 * @brief Platform shared memory primitive implementation
 *
 * Provides cross-platform shared memory mapping abstractions.
 * Uses POSIX shm_open/mmap on Unix/Linux/macOS and Win32 file mapping on Windows.
 */

#include "cobalt/platform/shm.h"
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
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#define COBALT_SHM_NAME_MAX 256

/**
 * @brief Opaque shared memory structure
 */
struct cobalt_shm {
#ifdef _WIN32
    HANDLE handle;
#else
    int fd;
#endif
    void  *addr;
    size_t size;
    char   name[COBALT_SHM_NAME_MAX];
};

/**
 * @brief Helper to format and normalize the shared memory name
 * @param name User-supplied name
 * @param buf Output buffer
 * @param buf_size Size of the output buffer
 * @return COBALT_SUCCESS on success, error code otherwise
 */
static int format_shm_name(const char *name, char *buf, size_t buf_size)
{
    if (!name || !buf || buf_size == 0) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    size_t len = strlen(name);
    if (len == 0) {
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
#ifdef _WIN32
    /* On Windows, strip any leading slashes */
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
#else
    if (name[0] == '/') {
        if (len >= buf_size) {
            return COBALT_ERROR_INVALID_ARGUMENT;
        }
        strncpy(buf, name, buf_size - 1);
        buf[buf_size - 1] = '\0';
    } else {
        if (len + 1 >= buf_size) {
            return COBALT_ERROR_INVALID_ARGUMENT;
        }
        int written = snprintf(buf, buf_size, "/%s", name);
        if (written < 0 || (size_t)written >= buf_size) {
            return COBALT_ERROR_INVALID_ARGUMENT;
        }
    }
    return COBALT_SUCCESS;
#endif
}

#ifdef _WIN32

int cobalt_shm_create(const char *name, size_t size, cobalt_shm_t **out_shm)
{
    if (!name || size == 0 || !out_shm) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    *out_shm = NULL;

    char win_name[COBALT_SHM_NAME_MAX];
    int  rc = format_shm_name(name, win_name, sizeof(win_name));
    if (rc != COBALT_SUCCESS) {
        cobalt_error_set(NULL, rc);
        return rc;
    }

    DWORD size_high = (DWORD)(((uint64_t)size >> 32) & 0xFFFFFFFF);
    DWORD size_low  = (DWORD)(size & 0xFFFFFFFF);

    HANDLE handle = CreateFileMappingA(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, size_high, size_low, win_name);
    if (!handle) {
        cobalt_error_set(NULL, COBALT_ERROR_IO);
        return COBALT_ERROR_IO;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(handle);
        cobalt_error_set(NULL, COBALT_ERROR_ALREADY_EXISTS);
        return COBALT_ERROR_ALREADY_EXISTS;
    }

    void *addr = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!addr) {
        CloseHandle(handle);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    cobalt_shm_t *shm = (cobalt_shm_t *)calloc(1, sizeof(cobalt_shm_t));
    if (!shm) {
        UnmapViewOfFile(addr);
        CloseHandle(handle);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    strncpy(shm->name, win_name, sizeof(shm->name) - 1);
    shm->name[sizeof(shm->name) - 1] = '\0';
    shm->handle                      = handle;
    shm->addr                        = addr;
    shm->size                        = size;

    *out_shm = shm;
    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

int cobalt_shm_open(const char *name, cobalt_shm_t **out_shm)
{
    if (!name || !out_shm) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    *out_shm = NULL;

    char win_name[COBALT_SHM_NAME_MAX];
    int  rc = format_shm_name(name, win_name, sizeof(win_name));
    if (rc != COBALT_SUCCESS) {
        cobalt_error_set(NULL, rc);
        return rc;
    }

    HANDLE handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, win_name);
    if (!handle) {
        DWORD err        = GetLastError();
        int   cobalt_err = (err == ERROR_FILE_NOT_FOUND) ? COBALT_ERROR_NOT_FOUND : COBALT_ERROR_IO;
        cobalt_error_set(NULL, cobalt_err);
        return cobalt_err;
    }

    void *addr = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!addr) {
        CloseHandle(handle);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    MEMORY_BASIC_INFORMATION mbi;
    size_t                   size = 0;
    if (VirtualQuery(addr, &mbi, sizeof(mbi))) {
        size = mbi.RegionSize;
    }

    cobalt_shm_t *shm = (cobalt_shm_t *)calloc(1, sizeof(cobalt_shm_t));
    if (!shm) {
        UnmapViewOfFile(addr);
        CloseHandle(handle);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    strncpy(shm->name, win_name, sizeof(shm->name) - 1);
    shm->name[sizeof(shm->name) - 1] = '\0';
    shm->handle                      = handle;
    shm->addr                        = addr;
    shm->size                        = size;

    *out_shm = shm;
    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

int cobalt_shm_unlink(const char *name)
{
    if (!name) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    char win_name[COBALT_SHM_NAME_MAX];
    int  rc = format_shm_name(name, win_name, sizeof(win_name));
    if (rc != COBALT_SUCCESS) {
        cobalt_error_set(NULL, rc);
        return rc;
    }
    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

#else /* POSIX */

int cobalt_shm_create(const char *name, size_t size, cobalt_shm_t **out_shm)
{
    if (!name || size == 0 || !out_shm) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    *out_shm = NULL;

    char path[COBALT_SHM_NAME_MAX];
    int  rc = format_shm_name(name, path, sizeof(path));
    if (rc != COBALT_SUCCESS) {
        cobalt_error_set(NULL, rc);
        return rc;
    }

    int fd = shm_open(path, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0666);
    if (fd < 0) {
        int err = COBALT_ERROR_IO;
        if (errno == EACCES) {
            err = COBALT_ERROR_PERMISSION_DENIED;
        } else if (errno == EEXIST) {
            err = COBALT_ERROR_ALREADY_EXISTS;
        } else if (errno == EINVAL) {
            err = COBALT_ERROR_INVALID_ARGUMENT;
        }
        cobalt_error_set(NULL, err);
        return err;
    }

    if (ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        shm_unlink(path);
        cobalt_error_set(NULL, COBALT_ERROR_IO);
        return COBALT_ERROR_IO;
    }

    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        shm_unlink(path);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    cobalt_shm_t *shm = (cobalt_shm_t *)calloc(1, sizeof(cobalt_shm_t));
    if (!shm) {
        munmap(addr, size);
        close(fd);
        shm_unlink(path);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    strncpy(shm->name, path, sizeof(shm->name) - 1);
    shm->name[sizeof(shm->name) - 1] = '\0';
    shm->fd                          = fd;
    shm->addr                        = addr;
    shm->size                        = size;

    *out_shm = shm;
    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

int cobalt_shm_open(const char *name, cobalt_shm_t **out_shm)
{
    if (!name || !out_shm) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }
    *out_shm = NULL;

    char path[COBALT_SHM_NAME_MAX];
    int  rc = format_shm_name(name, path, sizeof(path));
    if (rc != COBALT_SUCCESS) {
        cobalt_error_set(NULL, rc);
        return rc;
    }

    int fd = shm_open(path, O_RDWR | O_CLOEXEC, 0666);
    if (fd < 0) {
        int err = COBALT_ERROR_IO;
        if (errno == ENOENT) {
            err = COBALT_ERROR_NOT_FOUND;
        } else if (errno == EACCES) {
            err = COBALT_ERROR_PERMISSION_DENIED;
        } else if (errno == EINVAL) {
            err = COBALT_ERROR_INVALID_ARGUMENT;
        }
        cobalt_error_set(NULL, err);
        return err;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        close(fd);
        cobalt_error_set(NULL, COBALT_ERROR_IO);
        return COBALT_ERROR_IO;
    }

    size_t size = (size_t)st.st_size;
    void  *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    cobalt_shm_t *shm = (cobalt_shm_t *)calloc(1, sizeof(cobalt_shm_t));
    if (!shm) {
        munmap(addr, size);
        close(fd);
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return COBALT_ERROR_OUT_OF_MEMORY;
    }

    strncpy(shm->name, path, sizeof(shm->name) - 1);
    shm->name[sizeof(shm->name) - 1] = '\0';
    shm->fd                          = fd;
    shm->addr                        = addr;
    shm->size                        = size;

    *out_shm = shm;
    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

int cobalt_shm_unlink(const char *name)
{
    if (!name) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return COBALT_ERROR_INVALID_ARGUMENT;
    }

    char path[COBALT_SHM_NAME_MAX];
    int  rc = format_shm_name(name, path, sizeof(path));
    if (rc != COBALT_SUCCESS) {
        cobalt_error_set(NULL, rc);
        return rc;
    }

    if (shm_unlink(path) != 0) {
        int err = COBALT_ERROR_IO;
        if (errno == ENOENT) {
            err = COBALT_ERROR_NOT_FOUND;
        } else if (errno == EACCES) {
            err = COBALT_ERROR_PERMISSION_DENIED;
        }
        cobalt_error_set(NULL, err);
        return err;
    }

    cobalt_error_set(NULL, COBALT_SUCCESS);
    return COBALT_SUCCESS;
}

#endif /* _WIN32 */

void *cobalt_shm_get_addr(cobalt_shm_t *shm)
{
    return shm ? shm->addr : NULL;
}

size_t cobalt_shm_get_size(const cobalt_shm_t *shm)
{
    return shm ? shm->size : 0;
}

void cobalt_shm_close(cobalt_shm_t *shm)
{
    if (!shm) {
        return;
    }
#ifdef _WIN32
    if (shm->addr) {
        UnmapViewOfFile(shm->addr);
    }
    if (shm->handle && shm->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(shm->handle);
    }
#else
    if (shm->addr && shm->addr != MAP_FAILED) {
        munmap(shm->addr, shm->size);
    }
    if (shm->fd >= 0) {
        close(shm->fd);
    }
#endif
    free(shm);
}
