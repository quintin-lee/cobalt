#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
void test_set(void);
void test_deque(void);
void test_iterator(void);

/* Test function registry */
typedef struct
{
    const char* name;
    void (*func)(void);
} TestEntry;

static const TestEntry test_registry[] = {
    {"platform", test_platform}, {"atomic", test_atomic},       {"allocator", test_allocator},
    {"arena", test_arena},       {"error", test_error},         {"logger", test_logger},
    {"object", test_object},     {"class", test_class},         {"interface", test_interface},
    {"vector", test_vector},     {"list", test_list},           {"hashmap", test_hashmap},
    {"treemap", test_treemap},   {"sort", test_sort},           {"functional", test_functional},
    {"json", test_json},         {"eventloop", test_eventloop}, {"stack", test_stack},
    {"queue", test_queue},       {"set", test_set},             {"deque", test_deque},
    {"iterator", test_iterator},
};

static const int test_count = sizeof(test_registry) / sizeof(test_registry[0]);

static void run_all_tests(void)
{
    printf("=== Cobalt Unit Test Suite ===\n\n");

    for (int i = 0; i < test_count; i++)
        {
            printf("Testing %s...\n", test_registry[i].name);
            test_registry[i].func();
            printf("\n");
        }

    printf("=== All tests completed ===\n");
}

static void run_filtered_test(const char* filter)
{
    int found = 0;
    for (int i = 0; i < test_count; i++)
        {
            if (strcmp(test_registry[i].name, filter) == 0)
                {
                    printf("=== Cobalt Unit Test: %s ===\n\n", filter);
                    test_registry[i].func();
                    found = 1;
                    break;
                }
        }
    if (!found)
        {
            fprintf(stderr, "Error: Unknown test '%s'\n", filter);
            fprintf(stderr, "Available tests:\n");
            for (int i = 0; i < test_count; i++)
                {
                    fprintf(stderr, "  %s\n", test_registry[i].name);
                }
            exit(1);
        }
}

int main(int argc, char* argv[])
{
    if (argc > 1 && strcmp(argv[1], "--filter") == 0 && argc > 2)
        {
            run_filtered_test(argv[2]);
        }
    else
        {
            run_all_tests();
        }
    return 0;
}
