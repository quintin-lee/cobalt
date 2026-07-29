#include "platform/platform.h"
#include <string.h>

#ifdef _WIN32
#define COBALT_PLATFORM_WINDOWS 1
#elif __APPLE__
#define COBALT_PLATFORM_MACOS 1
#else
#define COBALT_PLATFORM_LINUX 1
#endif

cobalt_platform_id_t cobalt_platform_get_id(void) {
#ifdef COBALT_PLATFORM_WINDOWS
  return 1; /* Windows */
#elif COBALT_PLATFORM_MACOS
  return 2; /* macOS */
#else
  return 3; /* Linux */
#endif
}

cobalt_platform_handle_t cobalt_platform_get_handle(void) {
  return NULL; /* Placeholder */
}
