#ifndef PLATFORM_H
#define PLATFORM_H

/**
 * @file platform.h
 * @brief Platform abstraction layer
 */

#include <stddef.h>
#include <stdint.h>

/* Platform types */
typedef enum
{
    COBALT_PLATFORM_UNKNOWN = 0,
    COBALT_PLATFORM_WINDOWS = 1,
    COBALT_PLATFORM_MACOS = 2,
    COBALT_PLATFORM_LINUX = 3,
    COBALT_PLATFORM_OTHER = 99
} cobalt_platform_id_t;

typedef void* cobalt_platform_handle_t;

/* Platform detection */
#ifdef _WIN32
#define COBALT_PLATFORM_WINDOWS 1
#elif __APPLE__
#define COBALT_PLATFORM_MACOS 1
#else
#define COBALT_PLATFORM_LINUX 1
#endif

/* Platform initialization */
cobalt_platform_id_t cobalt_platform_get_id(void);
cobalt_platform_handle_t cobalt_platform_get_handle(void);

#endif /* PLATFORM_H */
