/**
 * @file test_json.c
 * @Unit test for JSON parsing and serialization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cobalt/module/json.h"

void test_json_parse_simple(void) {
    printf("Testing JSON parse simple values...\n");
    
    /* Parse a number */
    json_node_t *num = json_parse("42.5");
    if (num) {
        double val = json_get_number(num);
        if (val == 42.5) {
            printf("  Parse number 42.5: OK\n");
        } else {
            fprintf(stderr, "ERROR: Expected 42.5, got %f\n", val);
        }
        json_destroy(num);
    } else {
        fprintf(stderr, "ERROR: Failed to parse number\n");
    }
    
    /* Parse a string */
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
    
    /* Parse boolean true */
    json_node_t *t = json_parse("true");
    if (t) {
        if (json_is_null(t) == 0) {  /* Not null */
            printf("  Parse boolean true: OK\n");
        }
        json_destroy(t);
    }
    
    /* Parse null */
    json_node_t *n = json_parse("null");
    if (n && json_is_null(n)) {
        printf("  Parse null: OK\n");
        json_destroy(n);
    }
}

void test_json_serialize(void) {
    printf("Testing JSON serialize...\n");
    
    /* Create a simple node and serialize */
    json_node_t *node = json_parse("42");
    if (node) {
        char *out = json_serialize(node);
        if (out) {
            printf("  Serialize number: '%s'\n", out);
            free(out);
        }
        json_destroy(node);
    }
}

void test_json(void) {
    printf("Testing json...\n");
    test_json_parse_simple();
    test_json_serialize();
    printf("  JSON tests completed\n");
}
