#include "cobalt/algorithm/sort.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

void cobalt_qsort(void *base, size_t nmemb, size_t size, compare_func_t compar) {
  qsort(base, nmemb, size, compar);
}

void cobalt_insertion_sort(void *base, size_t nmemb, size_t size, compare_func_t compar) {
  /* Simple insertion sort */
  for (size_t i = 1; i < nmemb; i++) {
    void *key = (char *)base + i * size;
    int j = i - 1;
    while (j >= 0 && compar((char *)base + j * size, key) > 0) {
      *(void **)((char *)base + (j + 1) * size) = *(void **)((char *)base + j * size);
      j--;
    }
    *(void **)((char *)base + (j + 1) * size) = key;
  }
}

void cobalt_list_sort(void **head, size_t *count, compare_func_t compar) {
  (void)head; (void)count; (void)compar;
  /* Merge sort on linked list - placeholder */
}
