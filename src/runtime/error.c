#include "runtime/error.h"

const char *cobalt_error_get_message(cobalt_error_t code) {
  switch (code) {
    case COBALT_SUCCESS: return "Success";
    case COBALT_ERROR_GENERAL: return "General error";
    case COBALT_ERROR_INVALID_ARGUMENT: return "Invalid argument";
    case COBALT_ERROR_OUT_OF_MEMORY: return "Out of memory";
    case COBALT_ERROR_NOT_FOUND: return "Not found";
    case COBALT_ERROR_ALREADY_EXISTS: return "Already exists";
    case COBALT_ERROR_PERMISSION_DENIED: return "Permission denied";
    case COBALT_ERROR_IO: return "IO error";
    case COBALT_ERROR_TIMEOUT: return "Timeout";
    default: return "Unknown error";
  }
}

void cobalt_error_set(cobalt_error_t *error_ptr, cobalt_error_t code) {
  if (error_ptr) *error_ptr = code;
}

cobalt_error_t cobalt_error_get_current(void) {
  return COBALT_SUCCESS; /* Simplified */
}
