#ifndef PLATFORM_H
#define PLATFORM_H

/**
 * @file platform.h
 * @brief Platform abstraction layer
 * @details Provides OS platform identification and basic handle abstraction for cross-platform
 * compatibility.
 *
 * @defgroup Platform Platform abstraction module
 * @{
 */

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Platform identifier enumeration
 * @details Used to distinguish the current operating system platform at compile time or runtime.
 */
typedef enum {
    COBALT_PLATFORM_UNKNOWN = 0, /**< Unknown platform */
    COBALT_PLATFORM_WINDOWS = 1, /**< Windows platform */
    COBALT_PLATFORM_MACOS   = 2, /**< macOS platform */
    COBALT_PLATFORM_LINUX   = 3, /**< Linux platform */
    COBALT_PLATFORM_OTHER   = 99 /**< Other platform */
} cobalt_platform_id_t;

/**
 * @brief Platform-related generic handle type
 * @details This is an opaque pointer that can be cast to a specific platform's native handle (such
 * as HWND, Window, etc.).
 */
typedef void *cobalt_platform_handle_t;

/* Platform detection macros */
#ifdef _WIN32
/** @brief Defines the current platform as Windows */
#define COBALT_PLATFORM_WINDOWS 1
#elif __APPLE__
/** @brief Defines the current platform as macOS */
#define COBALT_PLATFORM_MACOS 1
#else
/** @brief Defines the current platform as Linux */
#define COBALT_PLATFORM_LINUX 1
#endif

/**
 * @brief Get the current platform identifier
 * @return cobalt_platform_id_t Enumeration value of the current platform
 * @note This function returns the platform identifier determined at compile time during runtime.
 */
cobalt_platform_id_t cobalt_platform_get_id(void);

/**
 * @brief Get the main handle of the current platform
 * @return cobalt_platform_handle_t The generic handle for the current platform, or NULL if it does
 * not exist.
 * @note The specific handle type returned depends on the underlying operating system and
 * application context.
 */
cobalt_platform_handle_t cobalt_platform_get_handle(void);

/** @} */

#endif /* PLATFORM_H */
