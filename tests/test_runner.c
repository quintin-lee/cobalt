/**
 * @file test_runner.c
 * Test runner for Cobalt unit tests.
 * Calls each module's test function and prints summary.
 */

#include <stdio.h>

/* Forward declarations - each test module provides these */
void test_platform(void);
void test_atomic(void);
void test_allocator(void);
void test_arena(void);
void test_error(void);
void test_logger(void);
void test_object(void);
void test_class(void);
void test_interface(void);
void test_vector(void);
void test_list(void);
void test_hashmap(void);
void test_treemap(void);
void test_sort(void);
void test_functional(void);
void test_json(void);
void test_eventloop(void);

int main(void) {
    printf("=== Cobalt Unit Test Suite (v2.0.0) ===\n\n");
    
    /* Platform tests */
    printf("--- Running test: platform ---\n");
    test_platform();
    printf("\n");
    
    /* Atomic tests */
    printf("--- Running test: atomic ---\n");
    test_atomic();
    printf("\n");
    
    /* Allocator tests */
    printf("--- Running test: allocator ---\n");
    test_allocator();
    printf("\n");
    
    /* Arena tests */
    printf("--- Running test: arena ---\n");
    test_arena();
    printf("\n");
    
    /* Error tests */
    printf("--- Running test: error ---\n");
    test_error();
    printf("\n");
    
    /* Logger tests */
    printf("--- Running test: logger ---\n");
    test_logger();
    printf("\n");
    
    /* Object tests */
    printf("--- Running test: object ---\n");
    test_object();
    printf("\n");
    
    /* Class tests */
    printf("--- Running test: class ---\n");
    test_class();
    printf("\n");
    
    /* Interface tests */
    printf("--- Running test: interface ---\n");
    test_interface();
    printf("\n");
    
    /* Vector tests */
    printf("--- Running test: vector ---\n");
    test_vector();
    printf("\n");
    
    /* List tests */
    printf("--- Running test: list ---\n");
    test_list();
    printf("\n");
    
    /* Hash map tests */
    printf("--- Running test: hashmap ---\n");
    test_hashmap();
    printf("\n");
    
    /* Tree map tests */
    printf("--- Running test: treemap ---\n");
    test_treemap();
    printf("\n");
    
    /* Sort tests */
    printf("--- Running test: sort ---\n");
    test_sort();
    printf("\n");
    
    /* Functional tests */
    printf("--- Running test: functional ---\n");
    test_functional();
    printf("\n");
    
    /* JSON tests */
    printf("--- Running test: json ---\n");
    test_json();
    printf("\n");
    
    /* Event loop tests */
    printf("--- Running test: eventloop ---\n");
    test_eventloop();
    printf("\n");
    
    printf("=== All tests completed ===\n");
    return 0;
}
