/**
 * @file cobalt.h
 * @brief Cobalt framework main header file
 *
 * This file includes all the core module header files of the Cobalt framework.
 * Users only need to include this single header file to use all features provided by the framework
 * (e.g., containers, memory management, runtime, etc.).
 */

#ifndef COBALT_H
#define COBALT_H

/* --- Platform and atomic operations --- */
#include <cobalt/platform/atomic.h>
#include <cobalt/platform/platform.h>

/* --- Memory management --- */
#include <cobalt/memory/allocator.h>
#include <cobalt/memory/arena.h>

/* --- Runtime and error handling --- */
#include <cobalt/runtime/error.h>
#include <cobalt/runtime/logger.h>

/* --- Core object-oriented system --- */
#include <cobalt/core/class.h>
#include <cobalt/core/interface.h>
#include <cobalt/core/object.h>

/* --- Abstract interfaces --- */
#include <cobalt/interface/iterator.h>
#include <cobalt/interface/sequence.h>
#include <cobalt/utils/foreach.h>
#include <cobalt/utils/string.h>

/* --- Container components --- */
#include <cobalt/container/deque.h>
#include <cobalt/container/hashmap.h>
#include <cobalt/container/list.h>
#include <cobalt/container/queue.h>
#include <cobalt/container/set.h>
#include <cobalt/container/stack.h>
#include <cobalt/container/treemap.h>
#include <cobalt/container/vector.h>

/* --- Algorithm support --- */
#include <cobalt/algorithm/functional.h>
#include <cobalt/algorithm/sort.h>

/* --- Additional function modules --- */
#include <cobalt/module/eventloop.h>
#include <cobalt/module/json.h>

#endif /* COBALT_H */
