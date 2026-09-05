/**
 * @file platform.c
 * @brief Implementation of the platform abstraction layer
 * @details Implements functionalities for identifying the current running platform and acquiring
 * the platform handle.
 */

#include "cobalt/platform/platform.h"

/**
 * @brief Get the ID of the currently running platform
 * @details Relies on preprocessor macros at compile time to determine the platform and returns the
 * corresponding enumerator.
 * @return Platform identifier (1 for Windows, 2 for macOS, 3 for Linux by default)
 */
cobalt_platform_id_t cobalt_platform_get_id(void)
{
#ifdef COBALT_PLATFORM_WINDOWS
    return 1; /* Return Windows platform identifier */
#elif COBALT_PLATFORM_MACOS
    return 2; /* Return macOS platform identifier */
#else
    return 3; /* Return Linux platform identifier by default */
#endif
}

/**
 * @brief Get the main platform handle
 * @details Returns NULL as no platform-specific handle is bound in this framework.
 * @return Platform handle, currently always NULL (not implemented)
 */
cobalt_platform_handle_t cobalt_platform_get_handle(void)
{
    // Getting the handle for a specific platform is not implemented yet, returning NULL for now
    return NULL; /* Placeholder */
}
