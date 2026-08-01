/**
 * @file test_json.c
 * @Unit test for JSON parsing and serialization.
 */

#include <stdio.h>
#include "cobalt/module/json.h"

void test_json(void) {
    printf("Testing json...\n");
    
    /* Basic parse test */
    json_node_t *num = json_parse("42");
    if (num) {
        double val = json_get_number(num);
        if (val == 42.0) {
            printf("  Parse number: OK\n");
        }
        json_destroy(num);
    }
    
    printf("  JSON tests completed\n");
}
