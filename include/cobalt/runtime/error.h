#ifndef ERROR_H
#define ERROR_H

/**
 * @file error.h
 * @brief Error handling framework module
 * @details Provides the definition of base error codes, error message retrieval, and thread-local error state management in the Cobalt framework.
 *
 * @defgroup RuntimeError Runtime error handling
 * @{
 */

#include <stdint.h>

/**
 * @brief Global error code enumeration for the Cobalt framework
 * @details Defines various error states that may occur, such as system operations, parameter validation, memory allocation, etc.
 */
typedef enum {
    COBALT_SUCCESS                 = 0,  /**< Success */
    COBALT_ERROR_GENERAL           = -1, /**< General error */
    COBALT_ERROR_INVALID_ARGUMENT  = -2, /**< Invalid argument */
    COBALT_ERROR_OUT_OF_MEMORY     = -3, /**< Out of memory */
    COBALT_ERROR_NOT_FOUND         = -4, /**< Not found */
    COBALT_ERROR_ALREADY_EXISTS    = -5, /**< Already exists */
    COBALT_ERROR_PERMISSION_DENIED = -6, /**< Permission denied */
    COBALT_ERROR_IO                = -7, /**< IO error */
    COBALT_ERROR_TIMEOUT           = -8, /**< Timeout */
} cobalt_error_t;

/**
 * @brief Get the readable error message string corresponding to the error code
 * 
 * @param code The error code to query
 * @return The corresponding error message description string
 */
const char *cobalt_error_get_message(cobalt_error_t code);

/**
 * @brief Set the current error state, and optionally write it to a pointer variable
 * @details This function not only assigns the error code to the variable pointed to by `error_ptr`,
 *          but also updates the thread-local error state of the current thread.
 * 
 * @param error_ptr Pointer to receive the error code; ignored if NULL
 * @param code The error code to set
 */
void cobalt_error_set(cobalt_error_t *error_ptr, cobalt_error_t code);

/**
 * @brief Get the last error code set by the current thread
 * 
 * @return The last error code of the current thread
 */
cobalt_error_t cobalt_error_get_current(void);

/** @} */

#endif /* ERROR_H */
