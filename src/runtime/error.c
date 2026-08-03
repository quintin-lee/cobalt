/**
 * @file error.c
 * @brief Implementation file of the error handling framework
 * @details Implements the error handling functions defined in error.h, including error message
 * retrieval and thread-local error state maintenance.
 */
#include "cobalt/runtime/error.h"
#include <threads.h>

/* Use C11 thread-local storage to record the last occurred error, ensuring thread safety */
static _Thread_local cobalt_error_t last_error = COBALT_SUCCESS;

/**
 * @brief Get the string information corresponding to the error code
 * @details Uses a switch statement to map known error codes, returning static strings.
 *
 * @param code Error code
 * @return Corresponding error description string
 */
const char *cobalt_error_get_message(cobalt_error_t code)
{
    switch (code) {
    case COBALT_SUCCESS:
        return "Success";
    case COBALT_ERROR_GENERAL:
        return "General error";
    case COBALT_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
    case COBALT_ERROR_OUT_OF_MEMORY:
        return "Out of memory";
    case COBALT_ERROR_NOT_FOUND:
        return "Not found";
    case COBALT_ERROR_ALREADY_EXISTS:
        return "Already exists";
    case COBALT_ERROR_PERMISSION_DENIED:
        return "Permission denied";
    case COBALT_ERROR_IO:
        return "IO error";
    case COBALT_ERROR_TIMEOUT:
        return "Timeout";
    case COBALT_ERROR_OUT_OF_BOUNDS:
        return "Index out of bounds";
    case COBALT_ERROR_EMPTY_CONTAINER:
        return "Container is empty";
    default:
        return "Unknown error";
    }
}

/**
 * @brief Set the given error pointer, and update the thread-local error state
 *
 * @param error_ptr Optional pointer to store the error code
 * @param code New error code
 */
void cobalt_error_set(cobalt_error_t *error_ptr, cobalt_error_t code)
{
    /* If a non-null pointer is passed in, assign it */
    if (error_ptr) {
        *error_ptr = code;
    }
    /* Always update the last error state of the current thread */
    last_error = code;
}

/**
 * @brief Get the last error code of the current thread
 *
 * @return cobalt_error_t The last error state of the current thread
 */
cobalt_error_t cobalt_error_get_current(void)
{
    return last_error;
}
