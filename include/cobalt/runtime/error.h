#ifndef ERROR_H
#define ERROR_H

/**
 * @file error.h
 * @brief Error handling framework
 */

#include <stdint.h>

/* Error codes */
typedef enum {
    COBALT_SUCCESS                 = 0,
    COBALT_ERROR_GENERAL           = -1,
    COBALT_ERROR_INVALID_ARGUMENT  = -2,
    COBALT_ERROR_OUT_OF_MEMORY     = -3,
    COBALT_ERROR_NOT_FOUND         = -4,
    COBALT_ERROR_ALREADY_EXISTS    = -5,
    COBALT_ERROR_PERMISSION_DENIED = -6,
    COBALT_ERROR_IO                = -7,
    COBALT_ERROR_TIMEOUT           = -8,
} cobalt_error_t;

/* Error message */
const char *cobalt_error_get_message(cobalt_error_t code);

/* Set current error */
void cobalt_error_set(cobalt_error_t *error_ptr, cobalt_error_t code);

/* Get current thread-local error */
cobalt_error_t cobalt_error_get_current(void);

#endif /* ERROR_H */
