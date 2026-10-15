#ifndef COBALT_MODULE_JSON_SAX_H
#define COBALT_MODULE_JSON_SAX_H

/**
 * @file json_sax.h
 * @brief High-performance Streaming SAX JSON Parser
 * @details Provides an event-driven SAX parser for JSON with zero heap allocation,
 *          O(1) memory overhead, 64-bit SWAR scanning, and early-exit capabilities.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum nesting depth for objects and arrays to prevent stack exhaustion.
 */
#define COBALT_JSON_MAX_DEPTH 128

/**
 * @brief SAX JSON parser error codes
 */
typedef enum cobalt_json_errcode {
    COBALT_JSON_OK = 0,                  /**< Parsing succeeded */
    COBALT_JSON_ERR_UNEXPECTED_CHAR,     /**< Unexpected character encountered */
    COBALT_JSON_ERR_UNTERMINATED_STRING, /**< String literal missing closing quote */
    COBALT_JSON_ERR_INVALID_ESCAPE,      /**< Invalid escape sequence in string */
    COBALT_JSON_ERR_INVALID_NUMBER,      /**< Malformed numeric literal */
    COBALT_JSON_ERR_UNBALANCED_BRACKETS, /**< Unmatched bracket or brace */
    COBALT_JSON_ERR_DEPTH_EXCEEDED,      /**< Nesting depth exceeds COBALT_JSON_MAX_DEPTH */
    COBALT_JSON_ERR_USER_ABORTED,        /**< Callback returned false to abort parse */
    COBALT_JSON_ERR_INVALID_UTF8,        /**< Malformed UTF-8 sequence */
    COBALT_JSON_ERR_UNEXPECTED_EOF,      /**< Input ended prematurely */
    COBALT_JSON_ERR_TRAILING_CONTENT     /**< Extra non-whitespace content after root value */
} cobalt_json_errcode_t;

/**
 * @brief Detailed error diagnostic structure
 */
typedef struct cobalt_json_error {
    cobalt_json_errcode_t code;         /**< Error code */
    size_t                offset;       /**< 0-based byte offset in input */
    size_t                line;         /**< 1-based line number */
    size_t                column;       /**< 1-based column number */
    char                  message[128]; /**< Human-readable diagnostic description */
} cobalt_json_error_t;

/**
 * @brief SAX event callback table
 * @details Callbacks return true to continue parsing, or false to abort immediately.
 *          Any callback pointer may be NULL to ignore that event.
 */
typedef struct cobalt_json_sax_handler {
    /** Called when 'null' literal is parsed */
    bool (*on_null)(void *ctx);

    /** Called when 'true' or 'false' literal is parsed */
    bool (*on_bool)(void *ctx, bool val);

    /**
     * Called when a number is parsed.
     * @param ctx User context
     * @param val Parsed double value
     * @param raw Raw text slice of the number
     * @param raw_len Length of raw text slice
     */
    bool (*on_number)(void *ctx, double val, const char *raw, size_t raw_len);

    /**
     * Called when a string value is parsed (unescaped).
     * @param ctx User context
     * @param val Unescaped string content (null-terminated)
     * @param len Byte length of unescaped string
     */
    bool (*on_string)(void *ctx, const char *val, size_t len);

    /**
     * Called when an object key is parsed (unescaped).
     * @param ctx User context
     * @param key Unescaped key content (null-terminated)
     * @param len Byte length of unescaped key
     */
    bool (*on_key)(void *ctx, const char *key, size_t len);

    /** Called when '{' is encountered */
    bool (*on_start_object)(void *ctx);

    /** Called when '}' is encountered */
    bool (*on_end_object)(void *ctx);

    /** Called when '[' is encountered */
    bool (*on_start_array)(void *ctx);

    /** Called when ']' is encountered */
    bool (*on_end_array)(void *ctx);
} cobalt_json_sax_handler_t;

/**
 * @brief Parse a JSON string in streaming SAX mode
 *
 * @param json_str Null-terminated or length-bounded JSON string (UTF-8)
 * @param len      Length of JSON string in bytes
 * @param handler  Table of event callback functions (may have NULL entries)
 * @param ctx      Caller-defined context pointer passed to callbacks
 * @param err      Optional diagnostic error output structure (may be NULL)
 * @return true on complete successful parse, false on syntax error or early abort
 */
bool cobalt_json_sax_parse(const char                      *json_str,
                           size_t                           len,
                           const cobalt_json_sax_handler_t *handler,
                           void                            *ctx,
                           cobalt_json_error_t             *err);

#ifdef __cplusplus
}
#endif

#endif /* COBALT_MODULE_JSON_SAX_H */
