#include "cobalt/platform/platform.h"

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
