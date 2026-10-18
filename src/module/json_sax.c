/**
 * @file json_sax.c
 * @brief Implementation of high-performance Streaming SAX JSON Parser
 */

#include "cobalt/module/json_sax.h"
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/* Internal Parser Types                                                     */
/* ========================================================================= */

typedef enum { FRAME_OBJECT = 0, FRAME_ARRAY = 1 } frame_type_t;

typedef struct {
    frame_type_t type;
    bool         has_items;    /**< True if at least one item has been parsed */
    bool         expect_value; /**< True if object key was parsed, expecting ':' and value */
} parser_frame_t;

typedef struct {
    const char                      *str;
    size_t                           pos;
    size_t                           len;
    size_t                           line;
    size_t                           col;
    const cobalt_json_sax_handler_t *handler;
    void                            *ctx;
    cobalt_json_error_t             *err;
    parser_frame_t                   stack[COBALT_JSON_MAX_DEPTH];
    size_t                           depth;
    bool                             aborted;
} sax_parser_t;

/* ========================================================================= */
/* SWAR Helpers                                                              */
/* ========================================================================= */

static inline uint64_t load64_le(const void *ptr)
{
    uint64_t v;
    memcpy(&v, ptr, sizeof(v));
    return v;
}

static inline uint64_t has_zero_byte(uint64_t v)
{
    return (v - 0x0101010101010101ULL) & ~v & 0x8080808080808080ULL;
}

static inline uint64_t has_byte(uint64_t v, uint8_t byte)
{
    uint64_t pattern = 0x0101010101010101ULL * byte;
    return has_zero_byte(v ^ pattern);
}

/* ========================================================================= */
/* Error Reporting                                                           */
/* ========================================================================= */

static bool set_error(sax_parser_t *p, cobalt_json_errcode_t code, const char *msg)
{
    if (p->err) {
        p->err->code   = code;
        p->err->offset = p->pos;
        p->err->line   = p->line;
        p->err->column = p->col;
        if (msg) {
            snprintf(p->err->message, sizeof(p->err->message), "%s", msg);
        } else {
            p->err->message[0] = '\0';
        }
    }
    return false;
}

/* ========================================================================= */
/* Whitespace Scanning                                                       */
/* ========================================================================= */

static void skip_whitespace(sax_parser_t *p)
{
    const char *s   = p->str;
    size_t      len = p->len;

    while (p->pos < len) {
        /* SWAR acceleration for 8 consecutive space characters (common in indentation) */
        if (p->pos + 8 <= len && load64_le(s + p->pos) == 0x2020202020202020ULL) {
            p->pos += 8;
            p->col += 8;
            continue;
        }

        char c = s[p->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            p->pos++;
            p->col++;
        } else if (c == '\n') {
            p->pos++;
            p->line++;
            p->col = 1;
        } else {
            break;
        }
    }
}

/* ========================================================================= */
/* String Scanning and Unescaping                                            */
/* ========================================================================= */

/**
 * @brief Fast-forward through non-escaped string characters using 64-bit SWAR
 */
static size_t scan_unescaped(const char *s, size_t pos, size_t len)
{
    while (pos + 8 <= len) {
        uint64_t w       = load64_le(s + pos);
        uint64_t m_quote = has_byte(w, '"');
        uint64_t m_slash = has_byte(w, '\\');
        uint64_t m_ctrl  = (w - 0x2020202020202020ULL) & ~w & 0x8080808080808080ULL;
        uint64_t mask    = m_quote | m_slash | m_ctrl;

        if (mask != 0) {
            unsigned long idx = (unsigned long)(__builtin_ctzll(mask) >> 3);
            return pos + idx;
        }
        pos += 8;
    }

    while (pos < len) {
        uint8_t c = (uint8_t)s[pos];
        if (c == '"' || c == '\\' || c < 0x20) {
            break;
        }
        pos++;
    }
    return pos;
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool parse_4hex(sax_parser_t *p, uint32_t *codepoint)
{
    if (p->pos + 4 > p->len) {
        return set_error(p, COBALT_JSON_ERR_INVALID_ESCAPE, "Incomplete \\u hex sequence");
    }
    uint32_t cp = 0;
    for (int i = 0; i < 4; i++) {
        int v = hex_val(p->str[p->pos++]);
        p->col++;
        if (v < 0) {
            return set_error(p, COBALT_JSON_ERR_INVALID_ESCAPE, "Invalid hex digit in \\u escape");
        }
        cp = (cp << 4) | (uint32_t)v;
    }
    *codepoint = cp;
    return true;
}

static size_t encode_utf8(uint32_t cp, char *out)
{
    if (cp <= 0x7F) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp <= 0x7FF) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp <= 0xFFFF) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp <= 0x10FFFF) {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

/**
 * @brief Parse a quoted string and invoke on_key or on_string
 */
static bool parse_string(sax_parser_t *p, bool is_key)
{
    /* Expect opening quote */
    if (p->pos >= p->len || p->str[p->pos] != '"') {
        return set_error(p, COBALT_JSON_ERR_UNEXPECTED_CHAR, "Expected opening quote '\"'");
    }
    p->pos++;
    p->col++;

    size_t start_pos = p->pos;

    /* Fast scan until quote, escape or EOF */
    size_t next_ctrl = scan_unescaped(p->str, p->pos, p->len);
    p->col += (next_ctrl - p->pos);
    p->pos = next_ctrl;

    /* Fast path: pure unescaped string */
    if (p->pos < p->len && p->str[p->pos] == '"') {
        size_t      raw_len = p->pos - start_pos;
        const char *raw_str = p->str + start_pos;
        p->pos++; /* Skip closing quote */
        p->col++;

        /* Terminate temporary string in-place if safe or copy */
        char  stack_buf[256];
        char *str_val;
        bool  allocated = false;

        if (raw_len < sizeof(stack_buf)) {
            memcpy(stack_buf, raw_str, raw_len);
            stack_buf[raw_len] = '\0';
            str_val            = stack_buf;
        } else {
            str_val = (char *)malloc(raw_len + 1);
            if (!str_val) {
                return set_error(p, COBALT_JSON_ERR_DEPTH_EXCEEDED, "Out of memory");
            }
            memcpy(str_val, raw_str, raw_len);
            str_val[raw_len] = '\0';
            allocated        = true;
        }

        bool ok = true;
        if (is_key) {
            if (p->handler->on_key) {
                ok = p->handler->on_key(p->ctx, str_val, raw_len);
            }
        } else {
            if (p->handler->on_string) {
                ok = p->handler->on_string(p->ctx, str_val, raw_len);
            }
        }

        if (allocated) {
            free(str_val);
        }

        if (!ok) {
            p->aborted = true;
            return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in string callback");
        }
        return true;
    }

    /* Slow path: contains escape sequences */
    char   stack_buf[4096];
    char  *buf       = stack_buf;
    size_t cap       = sizeof(stack_buf);
    size_t out_len   = 0;
    bool   allocated = false;

    /* Copy initial unescaped segment */
    size_t init_seg = p->pos - start_pos;
    if (init_seg > 0) {
        if (init_seg >= cap) {
            size_t need = init_seg > SIZE_MAX - 1 ? SIZE_MAX : init_seg + 1;
            cap         = need > SIZE_MAX / 2 ? SIZE_MAX : need * 2;
            buf         = (char *)malloc(cap);
            if (!buf) {
                return set_error(p, COBALT_JSON_ERR_DEPTH_EXCEEDED, "Out of memory");
            }
            allocated = true;
        }
        memcpy(buf, p->str + start_pos, init_seg);
        out_len = init_seg;
    }

    while (p->pos < p->len) {
        char c = p->str[p->pos];
        if (c == '"') {
            p->pos++;
            p->col++;
            break;
        }
        if (c == '\\') {
            p->pos++;
            p->col++;
            if (p->pos >= p->len) {
                if (allocated) {
                    free(buf);
                }
                return set_error(p, COBALT_JSON_ERR_INVALID_ESCAPE, "Unterminated escape sequence");
            }
            char esc = p->str[p->pos++];
            p->col++;

            char dec_char = 0;
            switch (esc) {
            case '"':
                dec_char = '"';
                break;
            case '\\':
                dec_char = '\\';
                break;
            case '/':
                dec_char = '/';
                break;
            case 'b':
                dec_char = '\b';
                break;
            case 'f':
                dec_char = '\f';
                break;
            case 'n':
                dec_char = '\n';
                break;
            case 'r':
                dec_char = '\r';
                break;
            case 't':
                dec_char = '\t';
                break;
            case 'u': {
                uint32_t cp = 0;
                if (!parse_4hex(p, &cp)) {
                    if (allocated) {
                        free(buf);
                    }
                    return false;
                }
                /* Check for UTF-16 surrogate pair */
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    if (p->pos + 2 <= p->len && p->str[p->pos] == '\\' &&
                        p->str[p->pos + 1] == 'u') {
                        p->pos += 2;
                        p->col += 2;
                        uint32_t low_cp = 0;
                        if (!parse_4hex(p, &low_cp)) {
                            if (allocated) {
                                free(buf);
                            }
                            return false;
                        }
                        if (low_cp >= 0xDC00 && low_cp <= 0xDFFF) {
                            cp = 0x10000 + (((cp - 0xD800) << 10) | (low_cp - 0xDC00));
                        } else {
                            if (allocated) {
                                free(buf);
                            }
                            return set_error(
                                p, COBALT_JSON_ERR_INVALID_ESCAPE, "Invalid UTF-16 surrogate pair");
                        }
                    }
                }
                char   utf8_seq[4];
                size_t ulen = encode_utf8(cp, utf8_seq);
                if (ulen == 0) {
                    if (allocated) {
                        free(buf);
                    }
                    return set_error(
                        p, COBALT_JSON_ERR_INVALID_ESCAPE, "Invalid Unicode codepoint");
                }
                if (out_len + ulen + 1 > cap) {
                    size_t need   = cap > SIZE_MAX - ulen - 1 ? SIZE_MAX : cap + ulen + 1;
                    cap           = need > SIZE_MAX / 2 ? SIZE_MAX : need * 2;
                    char *new_buf = (char *)realloc(allocated ? buf : NULL, cap);
                    if (!new_buf) {
                        if (allocated) {
                            free(buf);
                        }
                        return set_error(p, COBALT_JSON_ERR_DEPTH_EXCEEDED, "Out of memory");
                    }
                    if (!allocated) {
                        memcpy(new_buf, buf, out_len);
                    }
                    buf       = new_buf;
                    allocated = true;
                }
                memcpy(buf + out_len, utf8_seq, ulen);
                out_len += ulen;
                continue;
            }
            default:
                if (allocated) {
                    free(buf);
                }
                return set_error(p, COBALT_JSON_ERR_INVALID_ESCAPE, "Invalid escape character");
            }

            if (out_len + 2 > cap) {
                size_t need   = cap > SIZE_MAX - 2 ? SIZE_MAX : cap + 2;
                cap           = need > SIZE_MAX / 2 ? SIZE_MAX : need * 2;
                char *new_buf = (char *)realloc(allocated ? buf : NULL, cap);
                if (!new_buf) {
                    if (allocated) {
                        free(buf);
                    }
                    return set_error(p, COBALT_JSON_ERR_DEPTH_EXCEEDED, "Out of memory");
                }
                if (!allocated) {
                    memcpy(new_buf, buf, out_len);
                }
                buf       = new_buf;
                allocated = true;
            }
            buf[out_len++] = dec_char;
        } else if ((uint8_t)c < 0x20) {
            if (allocated) {
                free(buf);
            }
            return set_error(
                p, COBALT_JSON_ERR_UNEXPECTED_CHAR, "Unescaped control character in string");
        } else {
            /* Fast-forward remaining non-escaped chunk */
            size_t seg_start = p->pos;
            size_t seg_end   = scan_unescaped(p->str, p->pos, p->len);
            size_t seg_len   = seg_end - seg_start;
            p->col += seg_len;
            p->pos = seg_end;

            if (out_len + seg_len + 1 > cap) {
                size_t need   = cap > SIZE_MAX - seg_len - 1 ? SIZE_MAX : cap + seg_len + 1;
                cap           = need > SIZE_MAX / 2 ? SIZE_MAX : need * 2;
                char *new_buf = (char *)realloc(allocated ? buf : NULL, cap);
                if (!new_buf) {
                    if (allocated) {
                        free(buf);
                    }
                    return set_error(p, COBALT_JSON_ERR_DEPTH_EXCEEDED, "Out of memory");
                }
                if (!allocated) {
                    memcpy(new_buf, buf, out_len);
                }
                buf       = new_buf;
                allocated = true;
            }
            memcpy(buf + out_len, p->str + seg_start, seg_len);
            out_len += seg_len;
        }
    }

    if (p->pos > p->len || p->str[p->pos - 1] != '"') {
        if (allocated) {
            free(buf);
        }
        return set_error(p, COBALT_JSON_ERR_UNTERMINATED_STRING, "Unterminated string literal");
    }

    buf[out_len] = '\0';

    bool ok = true;
    if (is_key) {
        if (p->handler->on_key) {
            ok = p->handler->on_key(p->ctx, buf, out_len);
        }
    } else {
        if (p->handler->on_string) {
            ok = p->handler->on_string(p->ctx, buf, out_len);
        }
    }

    if (allocated) {
        free(buf);
    }

    if (!ok) {
        p->aborted = true;
        return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in string callback");
    }
    return true;
}

/* ========================================================================= */
/* Number Parsing                                                            */
/* ========================================================================= */

static bool parse_number(sax_parser_t *p)
{
    size_t start_pos = p->pos;
    size_t start_col = p->col;

    if (p->str[p->pos] == '-') {
        p->pos++;
        p->col++;
    }

    if (p->pos >= p->len) {
        return set_error(p, COBALT_JSON_ERR_INVALID_NUMBER, "Malformed number: lone minus");
    }

    /* Integer part */
    if (p->str[p->pos] == '0') {
        p->pos++;
        p->col++;
    } else if (p->str[p->pos] >= '1' && p->str[p->pos] <= '9') {
        while (p->pos < p->len && (p->str[p->pos] >= '0' && p->str[p->pos] <= '9')) {
            p->pos++;
            p->col++;
        }
    } else {
        return set_error(p, COBALT_JSON_ERR_INVALID_NUMBER, "Malformed number: expected digit");
    }

    /* Fractional part */
    if (p->pos < p->len && p->str[p->pos] == '.') {
        p->pos++;
        p->col++;
        if (p->pos >= p->len || !(p->str[p->pos] >= '0' && p->str[p->pos] <= '9')) {
            return set_error(
                p, COBALT_JSON_ERR_INVALID_NUMBER, "Malformed number: expected digit after '.'");
        }
        while (p->pos < p->len && (p->str[p->pos] >= '0' && p->str[p->pos] <= '9')) {
            p->pos++;
            p->col++;
        }
    }

    /* Exponent part */
    if (p->pos < p->len && (p->str[p->pos] == 'e' || p->str[p->pos] == 'E')) {
        p->pos++;
        p->col++;
        if (p->pos < p->len && (p->str[p->pos] == '+' || p->str[p->pos] == '-')) {
            p->pos++;
            p->col++;
        }
        if (p->pos >= p->len || !(p->str[p->pos] >= '0' && p->str[p->pos] <= '9')) {
            return set_error(
                p, COBALT_JSON_ERR_INVALID_NUMBER, "Malformed number: expected exponent digits");
        }
        while (p->pos < p->len && (p->str[p->pos] >= '0' && p->str[p->pos] <= '9')) {
            p->pos++;
            p->col++;
        }
    }

    size_t      raw_len = p->pos - start_pos;
    const char *raw_str = p->str + start_pos;

    /* Parse numeric value */
    char   stack_buf[64];
    char  *endptr;
    double val;

    if (raw_len < sizeof(stack_buf)) {
        memcpy(stack_buf, raw_str, raw_len);
        stack_buf[raw_len] = '\0';
        val                = strtod(stack_buf, &endptr);
    } else {
        char *tmp = (char *)malloc(raw_len + 1);
        if (!tmp) {
            return set_error(p, COBALT_JSON_ERR_DEPTH_EXCEEDED, "Out of memory");
        }
        memcpy(tmp, raw_str, raw_len);
        tmp[raw_len] = '\0';
        val          = strtod(tmp, &endptr);
        free(tmp);
    }

    if (p->handler->on_number) {
        if (!p->handler->on_number(p->ctx, val, raw_str, raw_len)) {
            p->aborted = true;
            p->col     = start_col;
            return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in on_number");
        }
    }
    return true;
}

/* ========================================================================= */
/* Recursive Descent Value Parser                                            */
/* ========================================================================= */

static bool parse_value(sax_parser_t *p);

static bool parse_object(sax_parser_t *p)
{
    if (p->depth >= COBALT_JSON_MAX_DEPTH) {
        return set_error(p, COBALT_JSON_ERR_DEPTH_EXCEEDED, "Exceeded maximum nesting depth (128)");
    }

    /* Consume '{' */
    p->pos++;
    p->col++;

    parser_frame_t *frame = &p->stack[p->depth++];
    frame->type           = FRAME_OBJECT;
    frame->has_items      = false;
    frame->expect_value   = false;

    if (p->handler->on_start_object) {
        if (!p->handler->on_start_object(p->ctx)) {
            p->aborted = true;
            return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in on_start_object");
        }
    }

    skip_whitespace(p);
    if (p->pos < p->len && p->str[p->pos] == '}') {
        p->pos++;
        p->col++;
        p->depth--;
        if (p->handler->on_end_object) {
            if (!p->handler->on_end_object(p->ctx)) {
                p->aborted = true;
                return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in on_end_object");
            }
        }
        return true;
    }

    while (p->pos < p->len) {
        skip_whitespace(p);
        if (p->pos >= p->len || p->str[p->pos] != '"') {
            return set_error(
                p, COBALT_JSON_ERR_UNEXPECTED_CHAR, "Expected string key in JSON object");
        }

        /* Parse Key */
        if (!parse_string(p, true)) {
            return false;
        }

        skip_whitespace(p);
        if (p->pos >= p->len || p->str[p->pos] != ':') {
            return set_error(p, COBALT_JSON_ERR_UNEXPECTED_CHAR, "Expected ':' after object key");
        }
        p->pos++;
        p->col++;

        /* Parse Value */
        skip_whitespace(p);
        if (!parse_value(p)) {
            return false;
        }
        frame->has_items = true;

        skip_whitespace(p);
        if (p->pos < p->len && p->str[p->pos] == ',') {
            p->pos++;
            p->col++;
            continue;
        }
        if (p->pos < p->len && p->str[p->pos] == '}') {
            p->pos++;
            p->col++;
            break;
        }

        return set_error(p, COBALT_JSON_ERR_UNEXPECTED_CHAR, "Expected ',' or '}' in JSON object");
    }

    p->depth--;
    if (p->handler->on_end_object) {
        if (!p->handler->on_end_object(p->ctx)) {
            p->aborted = true;
            return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in on_end_object");
        }
    }
    return true;
}

static bool parse_array(sax_parser_t *p)
{
    if (p->depth >= COBALT_JSON_MAX_DEPTH) {
        return set_error(p, COBALT_JSON_ERR_DEPTH_EXCEEDED, "Exceeded maximum nesting depth (128)");
    }

    /* Consume '[' */
    p->pos++;
    p->col++;

    parser_frame_t *frame = &p->stack[p->depth++];
    frame->type           = FRAME_ARRAY;
    frame->has_items      = false;
    frame->expect_value   = false;

    if (p->handler->on_start_array) {
        if (!p->handler->on_start_array(p->ctx)) {
            p->aborted = true;
            return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in on_start_array");
        }
    }

    skip_whitespace(p);
    if (p->pos < p->len && p->str[p->pos] == ']') {
        p->pos++;
        p->col++;
        p->depth--;
        if (p->handler->on_end_array) {
            if (!p->handler->on_end_array(p->ctx)) {
                p->aborted = true;
                return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in on_end_array");
            }
        }
        return true;
    }

    while (p->pos < p->len) {
        skip_whitespace(p);
        if (!parse_value(p)) {
            return false;
        }
        frame->has_items = true;

        skip_whitespace(p);
        if (p->pos < p->len && p->str[p->pos] == ',') {
            p->pos++;
            p->col++;
            continue;
        }
        if (p->pos < p->len && p->str[p->pos] == ']') {
            p->pos++;
            p->col++;
            break;
        }

        return set_error(p, COBALT_JSON_ERR_UNEXPECTED_CHAR, "Expected ',' or ']' in JSON array");
    }

    p->depth--;
    if (p->handler->on_end_array) {
        if (!p->handler->on_end_array(p->ctx)) {
            p->aborted = true;
            return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in on_end_array");
        }
    }
    return true;
}

static bool parse_value(sax_parser_t *p)
{
    skip_whitespace(p);
    if (p->pos >= p->len) {
        return set_error(p, COBALT_JSON_ERR_UNEXPECTED_EOF, "Unexpected end of input");
    }

    char c = p->str[p->pos];

    if (c == '{') {
        return parse_object(p);
    }
    if (c == '[') {
        return parse_array(p);
    }
    if (c == '"') {
        return parse_string(p, false);
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        return parse_number(p);
    }
    if (p->pos + 4 <= p->len && memcmp(p->str + p->pos, "true", 4) == 0) {
        p->pos += 4;
        p->col += 4;
        if (p->handler->on_bool) {
            if (!p->handler->on_bool(p->ctx, true)) {
                p->aborted = true;
                return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in on_bool");
            }
        }
        return true;
    }
    if (p->pos + 5 <= p->len && memcmp(p->str + p->pos, "false", 5) == 0) {
        p->pos += 5;
        p->col += 5;
        if (p->handler->on_bool) {
            if (!p->handler->on_bool(p->ctx, false)) {
                p->aborted = true;
                return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in on_bool");
            }
        }
        return true;
    }
    if (p->pos + 4 <= p->len && memcmp(p->str + p->pos, "null", 4) == 0) {
        p->pos += 4;
        p->col += 4;
        if (p->handler->on_null) {
            if (!p->handler->on_null(p->ctx)) {
                p->aborted = true;
                return set_error(p, COBALT_JSON_ERR_USER_ABORTED, "User aborted in on_null");
            }
        }
        return true;
    }

    return set_error(p, COBALT_JSON_ERR_UNEXPECTED_CHAR, "Unexpected character encountered");
}

/* ========================================================================= */
/* Public SAX API                                                            */
/* ========================================================================= */

bool cobalt_json_sax_parse(const char                      *json_str,
                           size_t                           len,
                           const cobalt_json_sax_handler_t *handler,
                           void                            *ctx,
                           cobalt_json_error_t             *err)
{
    if (err) {
        err->code       = COBALT_JSON_OK;
        err->offset     = 0;
        err->line       = 1;
        err->column     = 1;
        err->message[0] = '\0';
    }

    if (!json_str || !handler) {
        if (err) {
            err->code = COBALT_JSON_ERR_UNEXPECTED_EOF;
            snprintf(err->message, sizeof(err->message), "Invalid NULL argument");
        }
        return false;
    }

    sax_parser_t parser = {.str     = json_str,
                           .pos     = 0,
                           .len     = len,
                           .line    = 1,
                           .col     = 1,
                           .handler = handler,
                           .ctx     = ctx,
                           .err     = err,
                           .depth   = 0,
                           .aborted = false};

    skip_whitespace(&parser);
    if (parser.pos >= parser.len) {
        return set_error(&parser, COBALT_JSON_ERR_UNEXPECTED_EOF, "Empty JSON input");
    }

    if (!parse_value(&parser)) {
        return false;
    }

    skip_whitespace(&parser);
    if (parser.pos < parser.len) {
        return set_error(&parser,
                         COBALT_JSON_ERR_TRAILING_CONTENT,
                         "Unexpected trailing content after JSON root");
    }

    return true;
}
