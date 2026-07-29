#include "platform/atomic.h"

cobalt_atomic_t cobalt_atomic_create(int initial) {
  cobalt_atomic_t a = { .value = initial };
  return a;
}

int cobalt_atomic_get(cobalt_atomic_t *a) {
  return a->value;
}

void cobalt_atomic_set(cobalt_atomic_t *a, int value) {
  a->value = value;
}

void cobalt_atomic_increment(cobalt_atomic_t *a) {
  a->value++;
}

void cobalt_atomic_decrement(cobalt_atomic_t *a) {
  a->value--;
}
