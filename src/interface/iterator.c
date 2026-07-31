#include "cobalt/interface/iterator.h"
#include <stdlib.h>

typedef struct {
    cobalt_sequence_t *seq;
    size_t index;
    void *current;
} cobalt_iterator_impl_t;

cobalt_iterator_t *cobalt_iterator_new(cobalt_sequence_t *seq) {
    if (!seq) return NULL;
    
    cobalt_iterator_impl_t *iter = malloc(sizeof(cobalt_iterator_impl_t));
    if (!iter) return NULL;
    
    iter->seq = seq;
    iter->index = 0;
    iter->current = NULL;
    return (cobalt_iterator_t *)iter;
}

int cobalt_iterator_has_next(cobalt_iterator_t *iter) {
    if (!iter) return 0;
    cobalt_iterator_impl_t *impl = (cobalt_iterator_impl_t *)iter;
    return impl->index < impl->seq->size(impl->seq);
}

void *cobalt_iterator_next(cobalt_iterator_t *iter) {
    if (!iter) return NULL;
    cobalt_iterator_impl_t *impl = (cobalt_iterator_impl_t *)iter;
    
    if (impl->index >= impl->seq->size(impl->seq)) {
        return NULL;
    }
    
    /* For simplicity, use direct access - real impl would use sequence ops */
    (void)impl->current;
    impl->index++;
    return NULL; /* Stub - would return actual element */
}

void cobalt_iterator_destroy(cobalt_iterator_t *iter) {
    if (iter) {
        free(iter);
    }
}
