/**
 * @file cobalt.h
 * @brief Cobalt Framework Master Header
 *
 * Includes all core modules of the Cobalt framework.
 */

#ifndef COBALT_H
#define COBALT_H

#include <cobalt/platform/atomic.h>
#include <cobalt/platform/platform.h>

#include <cobalt/memory/allocator.h>
#include <cobalt/memory/arena.h>

#include <cobalt/runtime/error.h>
#include <cobalt/runtime/logger.h>

#include <cobalt/core/class.h>
#include <cobalt/core/interface.h>
#include <cobalt/core/object.h>

#include <cobalt/interface/iterator.h>
#include <cobalt/interface/map.h>
#include <cobalt/interface/sequence.h>

#include <cobalt/container/deque.h>
#include <cobalt/container/hashmap.h>
#include <cobalt/container/list.h>
#include <cobalt/container/queue.h>
#include <cobalt/container/set.h>
#include <cobalt/container/stack.h>
#include <cobalt/container/treemap.h>
#include <cobalt/container/vector.h>

#include <cobalt/algorithm/functional.h>
#include <cobalt/algorithm/sort.h>

#include <cobalt/module/eventloop.h>
#include <cobalt/module/json.h>

#endif /* COBALT_H */
