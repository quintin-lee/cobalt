/**
 * @file test_json.c
 * @brief Unit test for JSON parsing and serialization.
 */

#include "cobalt/module/json.h"
#include "test_framework.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_json_parse_null(void);
void test_json_parse_extended(void);
void test_json_fuzz(void);

void test_json_parse_null(void)
{
    printf("Testing JSON parse NULL...\n");

    json_node_t *node = json_parse(NULL);
    if (node == NULL) {
        printf("  Parse NULL returns NULL: OK\n");
    } else {
        fprintf(stderr, "ERROR: Expected NULL for NULL input\n");
        json_destroy(node);
    }

    json_node_t *empty = json_parse("");
    if (empty == NULL) {
        printf("  Parse empty string returns NULL: OK\n");
    } else {
        fprintf(stderr, "ERROR: Expected NULL for empty input\n");
        json_destroy(empty);
    }
}

void test_json_parse_number(void)
{
    printf("Testing JSON number parsing...\n");

    json_node_t *num = json_parse("42");
    if (num) {
        double val = json_get_number(num);
        if (val == 42.0) {
            printf("  Parse integer 42: OK\n");
        } else {
            fprintf(stderr, "ERROR: Expected 42.0, got %f\n", val);
        }
        json_destroy(num);
    } else {
        fprintf(stderr, "ERROR: Failed to parse number\n");
    }

    json_node_t *float_num = json_parse("3.14");
    if (float_num) {
        double val = json_get_number(float_num);
        if (fabs(val - 3.14) < 0.001) {
            printf("  Parse float 3.14: OK\n");
        } else {
            fprintf(stderr, "ERROR: Expected 3.14, got %f\n", val);
        }
        json_destroy(float_num);
    } else {
        fprintf(stderr, "ERROR: Failed to parse float\n");
    }

    json_node_t *neg = json_parse("-100");
    if (neg) {
        double val = json_get_number(neg);
        if (val == -100.0) {
            printf("  Parse negative number: OK\n");
        }
        json_destroy(neg);
    }
}

void test_json_parse_string(void)
{
    printf("Testing JSON string parsing...\n");

    json_node_t *str = json_parse("\"hello\"");
    if (str) {
        const char *val = json_get_string(str);
        if (val && strcmp(val, "hello") == 0) {
            printf("  Parse string \"hello\": OK\n");
        } else {
            fprintf(stderr, "ERROR: Expected \"hello\", got \"%s\"\n", val ? val : "NULL");
        }
        json_destroy(str);
    } else {
        fprintf(stderr, "ERROR: Failed to parse string\n");
    }

    /* Test escaped string */
    json_node_t *escaped = json_parse("\"hello\\nworld\"");
    if (escaped) {
        const char *val = json_get_string(escaped);
        if (val && strcmp(val, "hello\nworld") == 0) {
            printf("  Parse escaped newline: OK\n");
        } else {
            fprintf(stderr, "ERROR: Expected \"hello\\nworld\", got \"%s\"\n", val ? val : "NULL");
        }
        json_destroy(escaped);
    }

    /* Test tab escape */
    json_node_t *tab = json_parse("\"a\\tb\"");
    if (tab) {
        const char *val = json_get_string(tab);
        if (val && strcmp(val, "a\tb") == 0) {
            printf("  Parse escaped tab: OK\n");
        } else {
            fprintf(stderr, "ERROR: Expected \"a\\\\tb\", got \"%s\"\n", val ? val : "NULL");
        }
        json_destroy(tab);
    }

    /* Test backslash escape */
    json_node_t *bs = json_parse("\"C:\\\\Users\\\\test\"");
    if (bs) {
        const char *val = json_get_string(bs);
        if (val && strcmp(val, "C:\\Users\\test") == 0) {
            printf("  Parse escaped backslash: OK\n");
        } else {
            fprintf(stderr,
                    "ERROR: Expected \"C:\\\\Users\\\\test\", got \"%s\"\n",
                    val ? val : "NULL");
        }
        json_destroy(bs);
    }

    /* Test quote escape */
    json_node_t *qt = json_parse("\"say \\\"hello\\\"\"");
    if (qt) {
        const char *val = json_get_string(qt);
        if (val && strcmp(val, "say \"hello\"") == 0) {
            printf("  Parse escaped quote: OK\n");
        } else {
            fprintf(stderr,
                    "ERROR: Expected \"say \\\\\"hello\\\\\"\", got \"%s\"\n",
                    val ? val : "NULL");
        }
        json_destroy(qt);
    }
}

void test_json_parse_bool(void)
{
    printf("Testing JSON boolean parsing...\n");

    json_node_t *true_node = json_parse("true");
    if (true_node) {
        if (json_is_null(true_node) == 0) {
            printf("  Parse true: OK\n");
        }
        json_destroy(true_node);
    }

    json_node_t *false_node = json_parse("false");
    if (false_node) {
        if (json_is_null(false_node) == 0) {
            printf("  Parse false: OK\n");
        }
        json_destroy(false_node);
    }

    json_node_t *null_node = json_parse("null");
    if (null_node) {
        if (json_is_null(null_node)) {
            printf("  Parse null: OK\n");
        } else {
            fprintf(stderr, "ERROR: Expected null to be recognized\n");
        }
        json_destroy(null_node);
    }
}

void test_json_parse_array(void)
{
    printf("Testing JSON array parsing...\n");

    json_node_t *arr = json_parse("[1, 2, 3]");
    if (arr) {
        if (json_is_array(arr)) {
            printf("  Parse array: OK\n");
        } else {
            fprintf(stderr, "ERROR: Expected array type\n");
        }
        json_destroy(arr);
    } else {
        fprintf(stderr, "ERROR: Failed to parse array\n");
    }
}

void test_json_parse_object(void)
{
    printf("Testing JSON object parsing...\n");

    json_node_t *obj = json_parse("{\"name\": \"test\", \"value\": 42}");
    if (obj) {
        if (json_is_object(obj)) {
            printf("  Parse object: OK\n");
        } else {
            fprintf(stderr, "ERROR: Expected object type\n");
        }

        /* Test get_child */
        json_node_t *name = json_tree_get_child(obj, "name");
        if (name) {
            const char *val = json_get_string(name);
            if (val && strcmp(val, "test") == 0) {
                printf("  Get child \"name\": OK\n");
            }
        }

        json_node_t *value = json_tree_get_child(obj, "value");
        if (value) {
            double val = json_get_number(value);
            if (val == 42.0) {
                printf("  Get child \"value\": OK\n");
            }
        }

        json_node_t *missing = json_tree_get_child(obj, "missing");
        if (missing == NULL) {
            printf("  Get missing child returns NULL: OK\n");
        }

        json_destroy(obj);
    } else {
        fprintf(stderr, "ERROR: Failed to parse object\n");
    }
}

void test_json_serialize(void)
{
    printf("Testing JSON serialization...\n");

    /* Test serialize NULL */
    char *null_ser = json_serialize(NULL);
    if (null_ser == NULL) {
        printf("  Serialize NULL returns NULL: OK\n");
    }
    free(null_ser);

    /* Create a simple number and serialize */
    json_node_t *num = json_parse("42");
    if (num) {
        char *ser = json_serialize(num);
        if (ser) {
            printf("  Serialize number: \"%s\"\n", ser);
            free(ser);
        }
        json_destroy(num);
    }

    /* Create a simple string and serialize */
    json_node_t *str = json_parse("\"hello\"");
    if (str) {
        char *ser = json_serialize(str);
        if (ser) {
            printf("  Serialize string: \"%s\"\n", ser);
            free(ser);
        }
        json_destroy(str);
    }
}

void test_json_serialize_extended(void)
{
    printf("Testing JSON serialization (extended)...\n");

    /* Boolean serialization */
    json_node_t *true_node = json_parse("true");
    if (true_node) {
        char *true_ser = json_serialize(true_node);
        TEST_ASSERT(true_ser != NULL && strcmp(true_ser, "true") == 0);
        printf("  Serialize true: \"%s\" OK\n", true_ser);
        free(true_ser);
        json_destroy(true_node);
    }

    json_node_t *false_node = json_parse("false");
    if (false_node) {
        char *false_ser = json_serialize(false_node);
        TEST_ASSERT(false_ser != NULL && strcmp(false_ser, "false") == 0);
        printf("  Serialize false: \"%s\" OK\n", false_ser);
        free(false_ser);
        json_destroy(false_node);
    }

    /* Null serialization */
    json_node_t *null_node = json_parse("null");
    if (null_node) {
        char *null_ser2 = json_serialize(null_node);
        TEST_ASSERT(null_ser2 != NULL && strcmp(null_ser2, "null") == 0);
        printf("  Serialize null: \"%s\" OK\n", null_ser2);
        free(null_ser2);
        json_destroy(null_node);
    }

    /* Array serialization */
    json_node_t *arr = json_parse("[1, 2, 3]");
    if (arr) {
        char *arr_ser = json_serialize(arr);
        TEST_ASSERT(arr_ser != NULL);
        printf("  Serialize array: \"%s\" OK\n", arr_ser);
        free(arr_ser);
        json_destroy(arr);
    }

    /* Object serialization */
    json_node_t *obj = json_parse("{\"name\": \"test\", \"value\": 42}");
    if (obj) {
        char *obj_ser = json_serialize(obj);
        TEST_ASSERT(obj_ser != NULL);
        printf("  Serialize object: \"%s\" OK\n", obj_ser);
        free(obj_ser);
        json_destroy(obj);
    }

    /* String with special characters */
    json_node_t *esc = json_parse("\"hello\\nworld\\t\\\"test\\\"\"");
    if (esc) {
        char *esc_ser = json_serialize(esc);
        TEST_ASSERT(esc_ser != NULL);
        printf("  Serialize escaped string: \"%s\" OK\n", esc_ser);
        free(esc_ser);
        json_destroy(esc);
    }

    /* Empty string */
    json_node_t *empty_str = json_parse("\"\"");
    if (empty_str) {
        char *empty_ser = json_serialize(empty_str);
        TEST_ASSERT(empty_ser != NULL && strcmp(empty_ser, "\"\"") == 0);
        printf("  Serialize empty string: \"%s\" OK\n", empty_ser);
        free(empty_ser);
        json_destroy(empty_str);
    }

    printf("  Extended serialization tests completed\n");
}

void test_json_accessors(void)
{
    printf("Testing JSON accessors...\n");

    json_node_t *num = json_parse("42");
    if (num) {
        double val = json_get_number(num);
        printf("  Get number: %f\n", val);

        const char *str = json_get_string(num);
        printf("  Get string from number: %s\n", str ? str : "NULL");

        printf("  is_null: %d\n", json_is_null(num));
        printf("  is_object: %d\n", json_is_object(num));
        printf("  is_array: %d\n", json_is_array(num));

        json_destroy(num);
    }

    json_node_t *str_node = json_parse("\"test\"");
    if (str_node) {
        const char *val = json_get_string(str_node);
        printf("  Get string: %s\n", val ? val : "NULL");

        double num_val = json_get_number(str_node);
        printf("  Get number from string: %f\n", num_val);

        json_destroy(str_node);
    }
}

void test_json_destroy(void)
{
    printf("Testing JSON destroy...\n");

    /* Test destroy NULL */
    json_destroy(NULL);
    printf("  Destroy NULL: OK\n");

    /* Test destroy with various types */
    json_node_t *num = json_parse("42");
    json_destroy(num);
    printf("  Destroy number node: OK\n");

    json_node_t *str = json_parse("\"hello\"");
    json_destroy(str);
    printf("  Destroy string node: OK\n");

    json_node_t *arr = json_parse("[1, 2]");
    json_destroy(arr);
    printf("  Destroy array node: OK\n");

    json_node_t *obj = json_parse("{\"a\": 1}");
    json_destroy(obj);
    printf("  Destroy object node: OK\n");
}

void test_json(void)
{
    printf("Testing json...\n");
    test_json_parse_null();
    test_json_parse_number();
    test_json_parse_string();
    test_json_parse_bool();
    test_json_parse_array();
    test_json_parse_object();
    test_json_serialize();
    test_json_serialize_extended();
    test_json_parse_extended();
    test_json_accessors();
    test_json_destroy();
    test_json_fuzz();
    printf("  JSON tests completed\n");
}

void test_json_fuzz(void)
{
    printf("Testing JSON fuzzing (malformed inputs)...\n");

    json_node_t *node;

    node = json_parse("{");
    if (node) {
        json_destroy(node);
    }

    node = json_parse("[");
    if (node) {
        json_destroy(node);
    }

    node = json_parse("}");
    TEST_ASSERT(node == NULL);

    node = json_parse("]");
    TEST_ASSERT(node == NULL);

    node = json_parse("{{");
    if (node) {
        json_destroy(node);
    }

    node = json_parse("{}{}");
    if (node) {
        json_destroy(node);
    }

    node = json_parse("{\"a\":}");
    if (node) {
        json_destroy(node);
    }

    node = json_parse("{\"a\": 1,}");
    if (node) {
        json_destroy(node);
    }

    node = json_parse(": 1");
    TEST_ASSERT(node == NULL);

    node = json_parse("\"unclosed");
    TEST_ASSERT(node == NULL);

    node = json_parse("null\0null");
    if (node) {
        json_destroy(node);
    }

    node = json_parse("{\0}");
    if (node) {
        json_destroy(node);
    }

    node = json_parse("[\0]");
    if (node) {
        json_destroy(node);
    }

    node = json_parse("1\02");
    if (node) {
        json_destroy(node);
    }

    node = json_parse("true\0false");
    if (node) {
        json_destroy(node);
    }

    node = json_parse("   ");
    TEST_ASSERT(node == NULL);

    node = json_parse("\t\n\r");
    TEST_ASSERT(node == NULL);

    node = json_parse("42");
    TEST_ASSERT(node != NULL);
    json_destroy(node);

    node = json_parse("{\"a\": [1, 2, 3]}");
    TEST_ASSERT(node != NULL);
    json_destroy(node);

    printf("  Fuzzing: OK\n");
}

void test_json_parse_extended(void)
{
    printf("Testing JSON parsing (extended)...\n");

    /* Unicode escape */
    json_node_t *uni = json_parse("\"\\u0041\"");
    if (uni) {
        const char *val = json_get_string(uni);
        TEST_ASSERT(val != NULL && strcmp(val, "A") == 0);
        printf("  Unicode escape \\u0041: OK\n");
        json_destroy(uni);
    }

    /* Forward slash escape */
    json_node_t *slash = json_parse("\"http:\\\\/\\\\/example.com\"");
    if (slash) {
        const char *val = json_get_string(slash);
        TEST_ASSERT(val != NULL);
        printf("  Forward slash escape: OK\n");
        json_destroy(slash);
    }

    /* Backspace escape */
    json_node_t *bs = json_parse("\"back\\bspace\"");
    if (bs) {
        const char *val = json_get_string(bs);
        TEST_ASSERT(val != NULL && strlen(val) == 10);
        printf("  Backspace escape: OK\n");
        json_destroy(bs);
    }

    /* Form feed escape */
    json_node_t *ff = json_parse("\"form\\ffeed\"");
    if (ff) {
        const char *val = json_get_string(ff);
        TEST_ASSERT(val != NULL && strlen(val) == 9);
        printf("  Form feed escape: OK\n");
        json_destroy(ff);
    }

    /* Nested object */
    json_node_t *nested = json_parse("{\"outer\": {\"inner\": 42}}");
    if (nested) {
        json_node_t *outer = json_tree_get_child(nested, "outer");
        if (outer) {
            json_node_t *inner = json_tree_get_child(outer, "inner");
            if (inner) {
                double val = json_get_number(inner);
                TEST_ASSERT(val == 42.0);
                printf("  Nested object access: OK\n");
            }
        }
        json_destroy(nested);
    }

    /* Empty object */
    json_node_t *empty_obj = json_parse("{}");
    if (empty_obj) {
        TEST_ASSERT(json_is_object(empty_obj));
        json_node_t *child = json_tree_get_child(empty_obj, "any");
        TEST_ASSERT(child == NULL);
        printf("  Empty object: OK\n");
        json_destroy(empty_obj);
    }

    /* Empty array */
    json_node_t *empty_arr = json_parse("[]");
    if (empty_arr) {
        TEST_ASSERT(json_is_array(empty_arr));
        printf("  Empty array: OK\n");
        json_destroy(empty_arr);
    }

    /* Array with mixed types */
    json_node_t *mixed = json_parse("[1, \"two\", true, null]");
    if (mixed) {
        TEST_ASSERT(json_is_array(mixed));
        printf("  Mixed type array: OK\n");
        json_destroy(mixed);
    }

    /* Object with multiple keys */
    json_node_t *multi = json_parse("{\"a\":1,\"b\":2,\"c\":3}");
    if (multi) {
        TEST_ASSERT(json_is_object(multi));
        json_node_t *a = json_tree_get_child(multi, "a");
        json_node_t *b = json_tree_get_child(multi, "b");
        json_node_t *c = json_tree_get_child(multi, "c");
        TEST_ASSERT(a && json_get_number(a) == 1.0);
        TEST_ASSERT(b && json_get_number(b) == 2.0);
        TEST_ASSERT(c && json_get_number(c) == 3.0);
        printf("  Multi-key object: OK\n");
        json_destroy(multi);
    }

    /* Decimal number */
    json_node_t *dec = json_parse("42.5");
    if (dec) {
        double val = json_get_number(dec);
        TEST_ASSERT(fabs(val - 42.5) < 0.001);
        printf("  Decimal number: OK\n");
        json_destroy(dec);
    }

    printf("  Extended parsing tests completed\n");
}

