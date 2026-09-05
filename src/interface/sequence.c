/**
 * @file sequence.c
 * @brief Sequence interface convenience functions
 *
 * Provides standalone convenience wrappers for cobalt_sequence_t operations,
 * following the same pattern as the Map convenience API.
 */

#include "cobalt/interface/sequence.h"
#include <stdlib.h>

/* -------------------------------------------------------------------------- */
/* Convenience API                                                            */
/* -------------------------------------------------------------------------- */

/// @brief Get the number of elements (convenience wrapper)
/// @param seq Sequence instance
/// @return Number of elements, or 0 if seq is NULL

size_t cobalt_sequence_size(cobalt_sequence_t *seq)
{
    if (!seq || !seq->size) {
        return 0;
    }
    return seq->size(seq);
}

/// @brief Check if empty (convenience wrapper)
/// @param seq Sequence instance
/// @return Non-zero if empty, 0 otherwise

int cobalt_sequence_is_empty(cobalt_sequence_t *seq)
{
    if (!seq || !seq->is_empty) {
        return 1;
    }
    return seq->is_empty(seq);
}

/// @brief Add an element (convenience wrapper)
/// @param seq Sequence instance
/// @param item Element to add
/// @return 0 on success, -1 on failure
/// @note The backend `add` slot returns void (stable ABI), so failure is
/// detected the same way as cobalt_sequence_remove(): by comparing size
/// before and after. Backends MUST leave size unchanged when add fails.

int cobalt_sequence_add(cobalt_sequence_t *seq, void *item)
{
    if (!seq || !seq->add) {
        return -1;
    }
    size_t before = seq->size(seq);
    seq->add(seq, item);
    size_t after = seq->size(seq);
    return (after > before) ? 0 : -1;
}

/// @brief Remove an element by pointer equality (convenience wrapper)
/// @param seq Sequence instance
/// @param item Element to remove
/// @return 0 on success, -1 if not found

int cobalt_sequence_remove(cobalt_sequence_t *seq, void *item)
{
    if (!seq || !seq->remove) {
        return -1;
    }
    size_t before = seq->size(seq);
    seq->remove(seq, item);
    size_t after = seq->size(seq);
    return (after < before) ? 0 : -1;
}
