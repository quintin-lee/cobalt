/**
 * @file test_json.c
 * @Unit test for JSON parsing and serialization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cobalt/module/json.h"

void test_json_parse_values(void) {
    printf("Testing JSON parse basic values...\n");
    
    /* Number */
    json_node_t *num = json_parse("42.5");
    if (num) {
        double val = json_get_number(num);
        if (val == 42.5) {
            printf("  Parse number 42.5: OK\n");
        } else {
            fprintf(stderr, "ERROR: Expected 42.5, got %f\n", val);
        }
        json_destroy(num);
    }
    
    /* String */
    json_node_t *str = json_parse("\"hello\"");
    if (str) {
        const char *s = json_get_string(str);
        if (strcmp(s, "hello") == 0) {
            printf("  Parse string 'hello': OK\n");
        } else {
            fprintf(stderr, "ERROR: Expected 'hello', got '%s'\n", s);
        }
        json_destroy(str);
    }
    
    /* Boolean true */
    json_node_t *t = json_parse("true");
    if (t && !json_is_null(t)) {
        printf("  Parse boolean true: OK\n");
        json_destroy(t);
    }
    
    /* Boolean false */
    json_node_t *f = json_parse("false");
    if (f) {
        printf("  Parse boolean false: OK\n");
        json_destroy(f);
    }
    
    /* Null */
    json_node_t *n = json_parse("null");
    if (n && json_is_null(n)) {
        printf("  Parse null: OK\n");
        json_destroy(n);
    }
}

void test_json_parse_array(void) {
    printf("Testing JSON parse array...\n");
    
    json_node_t *arr = json_parse("[1, 2, 3]");
    if (arr && json_is_array(arr)) {
        printf("  Parse array: OK\n");
        json_destroy(arr);
    } else {
        fprintf(stderr, "ERROR: Failed to parse array\n");
    }
}

void test_json_parse_object(void) {
    printf("Testing JSON parse object...\n");
    
    json_node_t *obj = json_parse("{\"name\": \"Alice\", \"age\": 30}");
    if (obj && json_is_object(obj)) {
        printf("  Parse object: OK\n");
        json_destroy(obj);
    } else {
        fprintf(stderr, "ERROR: Failed to parse object\n");
    }
}

void test_json_serialize(void) {
    printf("Testing JSON serialize...\n");
    
    /* Serialize number */
    json_node_t *num = json_parse("42");
    if (num) {
        char *out = json_serialize(num);
        if (out) {
            printf("  Serialize number 42: '%s'\n", out);
            if (strcmp(out, "42") == 0) {
                printf("    Number serialization: OK\n");
            }
            free(out);
        }
        json_destroy(num);
    }
    
    /* Serialize string */
    json_node_t *str = json_parse("\"hello\"");
    if (str) {
        char *out = json_serialize(str);
        if (out) {
            printf("  Serialize string: '%s'\n", out);
            if (strcmp(out, "\"hello\"") == 0) {
                printf("    String serialization: OK\n");
            }
            free(out);
        }
        json_destroy(str);
    }
    
    /* Serialize array */
    json_node_t *arr = json_parse("[1, 2, 3]");
    if (arr) {
        char *out = json_serialize(arr);
        if (out) {
            printf("  Serialize array: '%s'\n", out);
            free(out);
        }
        json_destroy(arr);
    }
    
    /* Serialize object */
    json_node_t *obj = json_parse("{\"a\": 1}");
    if (obj) {
        char *out = json_serialize(obj);
        if (out) {
            printf("  Serialize object: '%s'\n", out);
            free(out);
        }
        json_destroy(obj);
    }
}

void test_json_roundtrip(void) {
    printf("Testing JSON roundtrip...\n");
    
    const char *original = "{\"name\": \"Alice\", \"age\": 30, \"active\": true}";
    json_node_t *node = json_parse(original);
    if (node) {
        char *serialized = json_serialize(node);
        if (serialized) {
            json_node_t *parsed_again = json_parse(serialized);
            if (parsed_again) {
                printf("  Roundtrip parse-serialize-parse: OK\n");
                json_destroy(parsed_again);
            }
            free(serialized);
        }
        json_destroy(node);
    }
}

void test_json(void) {
    printf("Testing json...\n");
    test_json_parse_values();
    test_json_parse_array();
    test_json_parse_object();
    test_json_serialize();
    test_json_roundtrip();
    printf("  JSON tests completed\n");
}
