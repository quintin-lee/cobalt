/**
 * @file test_json_sax.c
 * @brief Unit tests for Streaming SAX JSON Parser
 */

#include "cobalt/module/json_sax.h"
#include "test_framework.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EVENTS 64

typedef enum {
    EV_NULL,
    EV_BOOL,
    EV_NUM,
    EV_STR,
    EV_KEY,
    EV_START_OBJ,
    EV_END_OBJ,
    EV_START_ARR,
    EV_END_ARR
} event_type_t;

typedef struct {
    event_type_t type;
    bool         bool_val;
    double       num_val;
    char         str_val[128];
} record_event_t;

typedef struct {
    record_event_t events[MAX_EVENTS];
    size_t         count;
    const char    *abort_on_key;
    bool           abort_triggered;
} event_recorder_t;

static bool rec_on_null(void *ctx)
{
    event_recorder_t *rec = (event_recorder_t *)ctx;
    if (rec->count < MAX_EVENTS) {
        rec->events[rec->count++].type = EV_NULL;
    }
    return true;
}

static bool rec_on_bool(void *ctx, bool val)
{
    event_recorder_t *rec = (event_recorder_t *)ctx;
    if (rec->count < MAX_EVENTS) {
        record_event_t *ev = &rec->events[rec->count++];
        ev->type           = EV_BOOL;
        ev->bool_val       = val;
    }
    return true;
}

static bool rec_on_num(void *ctx, double val, const char *raw, size_t raw_len)
{
    (void)raw;
    (void)raw_len;
    event_recorder_t *rec = (event_recorder_t *)ctx;
    if (rec->count < MAX_EVENTS) {
        record_event_t *ev = &rec->events[rec->count++];
        ev->type           = EV_NUM;
        ev->num_val        = val;
    }
    return true;
}

static bool rec_on_str(void *ctx, const char *val, size_t len)
{
    (void)len;
    event_recorder_t *rec = (event_recorder_t *)ctx;
    if (rec->count < MAX_EVENTS) {
        record_event_t *ev = &rec->events[rec->count++];
        ev->type           = EV_STR;
        snprintf(ev->str_val, sizeof(ev->str_val), "%s", val);
    }
    return true;
}

static bool rec_on_key(void *ctx, const char *key, size_t len)
{
    (void)len;
    event_recorder_t *rec = (event_recorder_t *)ctx;
    if (rec->abort_on_key && strcmp(rec->abort_on_key, key) == 0) {
        rec->abort_triggered = true;
        return false; /* Trigger early-exit */
    }
    if (rec->count < MAX_EVENTS) {
        record_event_t *ev = &rec->events[rec->count++];
        ev->type           = EV_KEY;
        snprintf(ev->str_val, sizeof(ev->str_val), "%s", key);
    }
    return true;
}

static bool rec_on_start_obj(void *ctx)
{
    event_recorder_t *rec = (event_recorder_t *)ctx;
    if (rec->count < MAX_EVENTS) {
        rec->events[rec->count++].type = EV_START_OBJ;
    }
    return true;
}

static bool rec_on_end_obj(void *ctx)
{
    event_recorder_t *rec = (event_recorder_t *)ctx;
    if (rec->count < MAX_EVENTS) {
        rec->events[rec->count++].type = EV_END_OBJ;
    }
    return true;
}

static bool rec_on_start_arr(void *ctx)
{
    event_recorder_t *rec = (event_recorder_t *)ctx;
    if (rec->count < MAX_EVENTS) {
        rec->events[rec->count++].type = EV_START_ARR;
    }
    return true;
}

static bool rec_on_end_arr(void *ctx)
{
    event_recorder_t *rec = (event_recorder_t *)ctx;
    if (rec->count < MAX_EVENTS) {
        rec->events[rec->count++].type = EV_END_ARR;
    }
    return true;
}

static const cobalt_json_sax_handler_t test_handler = {
    .on_null         = rec_on_null,
    .on_bool         = rec_on_bool,
    .on_number       = rec_on_num,
    .on_string       = rec_on_str,
    .on_key          = rec_on_key,
    .on_start_object = rec_on_start_obj,
    .on_end_object   = rec_on_end_obj,
    .on_start_array  = rec_on_start_arr,
    .on_end_array    = rec_on_end_arr,
};

static void test_sax_scalars(void)
{
    printf("Testing SAX scalar parsing...\n");
    event_recorder_t    rec;
    cobalt_json_error_t err;

    /* null */
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(cobalt_json_sax_parse("null", 4, &test_handler, &rec, &err));
    TEST_ASSERT(rec.count == 1 && rec.events[0].type == EV_NULL);

    /* true */
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(cobalt_json_sax_parse("true", 4, &test_handler, &rec, &err));
    TEST_ASSERT(rec.count == 1 && rec.events[0].type == EV_BOOL && rec.events[0].bool_val == true);

    /* false */
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(cobalt_json_sax_parse("false", 5, &test_handler, &rec, &err));
    TEST_ASSERT(rec.count == 1 && rec.events[0].type == EV_BOOL && rec.events[0].bool_val == false);

    /* number */
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(cobalt_json_sax_parse("-42.5e2", 7, &test_handler, &rec, &err));
    TEST_ASSERT(rec.count == 1 && rec.events[0].type == EV_NUM);
    TEST_ASSERT(fabs(rec.events[0].num_val - (-4250.0)) < 0.001);

    /* string */
    memset(&rec, 0, sizeof(rec));
    const char *s = "\"Hello World\"";
    TEST_ASSERT(cobalt_json_sax_parse(s, strlen(s), &test_handler, &rec, &err));
    TEST_ASSERT(rec.count == 1 && rec.events[0].type == EV_STR);
    TEST_ASSERT(strcmp(rec.events[0].str_val, "Hello World") == 0);

    printf("  SAX scalars: OK\n");
}

static void test_sax_containers(void)
{
    printf("Testing SAX container parsing...\n");
    event_recorder_t    rec;
    cobalt_json_error_t err;

    /* Empty object */
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(cobalt_json_sax_parse("{}", 2, &test_handler, &rec, &err));
    TEST_ASSERT(rec.count == 2);
    TEST_ASSERT(rec.events[0].type == EV_START_OBJ && rec.events[1].type == EV_END_OBJ);

    /* Empty array */
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(cobalt_json_sax_parse("[]", 2, &test_handler, &rec, &err));
    TEST_ASSERT(rec.count == 2);
    TEST_ASSERT(rec.events[0].type == EV_START_ARR && rec.events[1].type == EV_END_ARR);

    /* Nested structure */
    const char *json = "{\"name\": \"Alice\", \"scores\": [10, 20]}";
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(cobalt_json_sax_parse(json, strlen(json), &test_handler, &rec, &err));
    TEST_ASSERT(rec.count == 9);
    TEST_ASSERT(rec.events[0].type == EV_START_OBJ);
    TEST_ASSERT(rec.events[1].type == EV_KEY && strcmp(rec.events[1].str_val, "name") == 0);
    TEST_ASSERT(rec.events[2].type == EV_STR && strcmp(rec.events[2].str_val, "Alice") == 0);
    TEST_ASSERT(rec.events[3].type == EV_KEY && strcmp(rec.events[3].str_val, "scores") == 0);
    TEST_ASSERT(rec.events[4].type == EV_START_ARR);
    TEST_ASSERT(rec.events[5].type == EV_NUM && rec.events[5].num_val == 10.0);
    TEST_ASSERT(rec.events[6].type == EV_NUM && rec.events[6].num_val == 20.0);
    TEST_ASSERT(rec.events[7].type == EV_END_ARR);
    TEST_ASSERT(rec.events[8].type == EV_END_OBJ);

    printf("  SAX containers: OK\n");
}

static void test_sax_escapes_and_unicode(void)
{
    printf("Testing SAX string escapes and Unicode...\n");
    event_recorder_t    rec;
    cobalt_json_error_t err;

    /* Escape sequences */
    const char *json_esc = "\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u0041\"";
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(cobalt_json_sax_parse(json_esc, strlen(json_esc), &test_handler, &rec, &err));
    TEST_ASSERT(rec.count == 1 && rec.events[0].type == EV_STR);
    TEST_ASSERT(strcmp(rec.events[0].str_val, "\"\\/\b\f\n\r\tA") == 0);

    /* Surrogate pair: \uD83D\uDE00 -> Grinning Face (U+1F600) -> UTF-8: F0 9F 98 80 */
    const char *json_surr = "\"\\uD83D\\uDE00\"";
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(cobalt_json_sax_parse(json_surr, strlen(json_surr), &test_handler, &rec, &err));
    TEST_ASSERT(rec.count == 1 && rec.events[0].type == EV_STR);
    unsigned char *u8 = (unsigned char *)rec.events[0].str_val;
    TEST_ASSERT(u8[0] == 0xF0 && u8[1] == 0x9F && u8[2] == 0x98 && u8[3] == 0x80);

    printf("  SAX escapes and Unicode: OK\n");
}

static void test_sax_early_exit(void)
{
    printf("Testing SAX early-exit upon callback returning false...\n");
    event_recorder_t    rec;
    cobalt_json_error_t err;

    const char *json = "{\"a\": 1, \"stop_here\": 2, \"c\": 3}";
    memset(&rec, 0, sizeof(rec));
    rec.abort_on_key = "stop_here";

    bool ret = cobalt_json_sax_parse(json, strlen(json), &test_handler, &rec, &err);
    TEST_ASSERT(!ret);
    TEST_ASSERT(rec.abort_triggered);
    TEST_ASSERT(err.code == COBALT_JSON_ERR_USER_ABORTED);

    printf("  SAX early-exit: OK\n");
}

static void test_sax_depth_and_errors(void)
{
    printf("Testing SAX depth limits and error handling...\n");
    event_recorder_t    rec;
    cobalt_json_error_t err;

    /* Unterminated string */
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(!cobalt_json_sax_parse("\"unterminated", 13, &test_handler, &rec, &err));
    TEST_ASSERT(err.code == COBALT_JSON_ERR_UNTERMINATED_STRING);

    /* Lone minus */
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(!cobalt_json_sax_parse("-", 1, &test_handler, &rec, &err));
    TEST_ASSERT(err.code == COBALT_JSON_ERR_INVALID_NUMBER);

    /* Trailing comma in array */
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(!cobalt_json_sax_parse("[1, ]", 5, &test_handler, &rec, &err));
    TEST_ASSERT(err.code == COBALT_JSON_ERR_UNEXPECTED_CHAR);

    /* Missing colon in object */
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(!cobalt_json_sax_parse("{\"key\" \"value\"}", 15, &test_handler, &rec, &err));
    TEST_ASSERT(err.code == COBALT_JSON_ERR_UNEXPECTED_CHAR);

    /* Trailing content */
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(!cobalt_json_sax_parse("[1] extra", 9, &test_handler, &rec, &err));
    TEST_ASSERT(err.code == COBALT_JSON_ERR_TRAILING_CONTENT);

    /* Depth limit exceeded (>128) */
    char deep[300];
    for (int i = 0; i < 140; i++) {
        deep[i] = '[';
    }
    for (int i = 0; i < 140; i++) {
        deep[140 + i] = ']';
    }
    deep[280] = '\0';
    memset(&rec, 0, sizeof(rec));
    TEST_ASSERT(!cobalt_json_sax_parse(deep, 280, &test_handler, &rec, &err));
    TEST_ASSERT(err.code == COBALT_JSON_ERR_DEPTH_EXCEEDED);

    printf("  SAX depth and errors: OK\n");
}

void test_json_sax(void)
{
    printf("=== Streaming SAX JSON Parser Tests ===\n");
    test_sax_scalars();
    test_sax_containers();
    test_sax_escapes_and_unicode();
    test_sax_early_exit();
    test_sax_depth_and_errors();
    printf("=== All SAX Parser Tests Passed ===\n");
}
