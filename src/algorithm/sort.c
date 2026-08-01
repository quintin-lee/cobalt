#include "cobalt/algorithm/sort.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void cobalt_qsort(void* base, size_t nmemb, size_t size, compare_func_t compar)
{
    qsort(base, nmemb, size, compar);
}

void cobalt_insertion_sort(void* base, size_t nmemb, size_t size, compare_func_t compar)
{
    if (!base || nmemb <= 1)
        return;

    char* arr = (char*)base;
    char* key = malloc(size);
    if (!key)
        return;

    for (size_t i = 1; i < nmemb; i++)
        {
            memcpy(key, arr + i * size, size);
            int j = (int)i - 1;

            while (j >= 0 && compar(arr + j * size, key) > 0)
                {
                    memcpy(arr + (j + 1) * size, arr + j * size, size);
                    j--;
                }
            memcpy(arr + (j + 1) * size, key, size);
        }

    free(key);
}

void cobalt_list_sort(void** head, size_t* count, compare_func_t compar)
{
    (void)head;
    (void)count;
    (void)compar;
    /* Merge sort on linked list - placeholder */
}
