#include <stdio.h>

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
void test_stack(void);
void test_queue(void);

int main(void) {
    printf("=== Cobalt Unit Test Suite ===\n\n");
    
    test_platform();
    test_atomic();
    test_allocator();
    test_arena();
    test_error();
    test_logger();
    test_object();
    test_class();
    test_interface();
    test_vector();
    test_list();
    test_hashmap();
    test_treemap();
    test_sort();
    test_functional();
    test_json();
    test_eventloop();
    test_stack();
    test_queue();
    
    printf("\n=== All tests completed ===\n");
    return 0;
}
