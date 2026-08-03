/**
 * @file test_json.c
 * @brief Unit test for JSON parsing and serialization.
 */

#include "cobalt/module/json.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        if (val) {
            printf("  Parse escaped string: OK (got \"%s\")\n", val);
        }
        json_destroy(escaped);
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
    test_json_accessors();
    test_json_destroy();
    printf("  JSON tests completed\n");
}
