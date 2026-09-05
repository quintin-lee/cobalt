#ifndef PLATFORM_UTILS_H
#define PLATFORM_UTILS_H

/**
 * @file utils.h
 * @brief Platform utility macros and functions
 * @details Provides portable alignment helpers and endianness conversion utilities.
 *
 * @defgroup PlatformUtils Platform utilities
 * @ingroup Platform
 * @{
 */

#include <stdint.h>
#include <string.h>

/* ========================================================================= */
/* Alignment helpers                                                          */
/* ========================================================================= */

/**
 * @brief Align a value up to the given boundary (must be a power of two)
 * @param val      Value to align
 * @param boundary Alignment boundary (e.g. 8, 16, 64)
 * @return Rounded-up aligned value
 */
#define cobalt_align(val, boundary) (((val) + ((boundary) - 1)) & ~((boundary) - 1))

/**
 * @brief Get the alignment offset needed for a value
 * @param val      Original value
 * @param boundary Alignment boundary
 * @return Number of padding bytes needed
 */
#define cobalt_align_offset(val, boundary) (cobalt_align((val), (boundary)) - (val))

/**
 * @brief Check if a value is already aligned
 * @param val      Value to check
 * @param boundary Alignment boundary
 * @return 1 if aligned, 0 otherwise
 */
#define cobalt_is_aligned(val, boundary) (((val) & ((boundary) - 1)) == 0)

/* ========================================================================= */
/* Endianness detection                                                       */
/* ========================================================================= */

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) &&                                 \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define COBALT_HOST_IS_LITTLE_ENDIAN 1
#define COBALT_HOST_IS_BIG_ENDIAN 0
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) &&                                  \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define COBALT_HOST_IS_LITTLE_ENDIAN 0
#define COBALT_HOST_IS_BIG_ENDIAN 1
#elif defined(_WIN32)
#define COBALT_HOST_IS_LITTLE_ENDIAN 1
#define COBALT_HOST_IS_BIG_ENDIAN 0
#else
/* Fallback: detect at runtime via helper */
/** Host is little-endian */
#define COBALT_HOST_IS_LITTLE_ENDIAN 0
/** Host is big-endian */
#define COBALT_HOST_IS_BIG_ENDIAN 0
#endif

/**
 * @name Byte-swap macros
 * @brief Platform-independent byte-swap operations
 * @{
 */
/* ========================================================================= */
/* Byte-swap builtins (GCC/Clang)                                             */
/* ========================================================================= */

#if defined(__GNUC__) || defined(__clang__)
#define COBALT_BSWAP16(x) __builtin_bswap16(x)
#define COBALT_BSWAP32(x) __builtin_bswap32(x)
#define COBALT_BSWAP64(x) __builtin_bswap64(x)
#else
/* Portable fallback implementations */
static inline uint16_t cobalt_bswap16_generic(uint16_t x)
{
    return (uint16_t)(((x >> 8) & 0xFFu) | ((x & 0xFFu) << 8));
}
static inline uint32_t cobalt_bswap32_generic(uint32_t x)
{
    return ((x >> 24) & 0xFFu) | ((x >> 8) & 0xFF00u) | ((x & 0xFF00u) << 8) | ((x & 0xFFu) << 24);
}
static inline uint64_t cobalt_bswap64_generic(uint64_t x)
{
    return ((uint64_t)cobalt_bswap32_generic((uint32_t)x) << 32) |
           cobalt_bswap32_generic((uint32_t)(x >> 32));
}
/** Byte-swap 16-bit integer */
#define COBALT_BSWAP16 cobalt_bswap16_generic
/** Byte-swap 32-bit integer */
#define COBALT_BSWAP32 cobalt_bswap32_generic
/** Byte-swap 64-bit integer */
#define COBALT_BSWAP64 cobalt_bswap64_generic
#endif
/** @} */

/* ========================================================================= */
/* Host <-> Network byte order conversion                                     */
/* ========================================================================= */

/**
 * @brief Convert uint16 from host to network (big-endian) byte order
 */
static inline uint16_t cobalt_host_to_net16(uint16_t host)
{
#if COBALT_HOST_IS_LITTLE_ENDIAN
    return COBALT_BSWAP16(host);
#elif COBALT_HOST_IS_BIG_ENDIAN
    return host;
#else
    /* Runtime detection fallback */
    uint16_t test  = 0x0102;
    uint8_t *bytes = (uint8_t *)&test;
    return bytes[0] == 0x01 ? host : COBALT_BSWAP16(host);
#endif
}

/**
 * @brief Convert uint16 from network (big-endian) to host byte order
 */
static inline uint16_t cobalt_net_to_host16(uint16_t net)
{
    return cobalt_host_to_net16(net);
}

/**
 * @brief Convert uint32 from host to network (big-endian) byte order
 */
static inline uint32_t cobalt_host_to_net32(uint32_t host)
{
#if COBALT_HOST_IS_LITTLE_ENDIAN
    return COBALT_BSWAP32(host);
#elif COBALT_HOST_IS_BIG_ENDIAN
    return host;
#else
    uint16_t test  = 0x0102;
    uint8_t *bytes = (uint8_t *)&test;
    return bytes[0] == 0x01 ? host : COBALT_BSWAP32(host);
#endif
}

/**
 * @brief Convert uint32 from network (big-endian) to host byte order
 */
static inline uint32_t cobalt_net_to_host32(uint32_t net)
{
    return cobalt_host_to_net32(net);
}

/**
 * @brief Convert uint64 from host to network (big-endian) byte order
 */
static inline uint64_t cobalt_host_to_net64(uint64_t host)
{
#if COBALT_HOST_IS_LITTLE_ENDIAN
    return COBALT_BSWAP64(host);
#elif COBALT_HOST_IS_BIG_ENDIAN
    return host;
#else
    uint16_t test  = 0x0102;
    uint8_t *bytes = (uint8_t *)&test;
    return bytes[0] == 0x01 ? host : COBALT_BSWAP64(host);
#endif
}

/**
 * @brief Convert uint64 from network (big-endian) to host byte order
 */
static inline uint64_t cobalt_net_to_host64(uint64_t net)
{
    return cobalt_host_to_net64(net);
}

/**
 * @brief Convert between host and network byte order for any integer size
 * @param val  Value to convert
 * @return     Byte-swapped value on little-endian, same value on big-endian
 */
static inline uint16_t cobalt_swap16(uint16_t val)
{
    return COBALT_BSWAP16(val);
}

/**
 * @brief Convert between host and network byte order for 32-bit integers
 * @param val  Value to convert
 * @return     Byte-swapped value on little-endian, same value on big-endian
 */
static inline uint32_t cobalt_swap32(uint32_t val)
{
    return COBALT_BSWAP32(val);
}

/**
 * @brief Convert between host and network byte order for 64-bit integers
 * @param val  Value to convert
 * @return     Byte-swapped value on little-endian, same value on big-endian
 */
static inline uint64_t cobalt_swap64(uint64_t val)
{
    return COBALT_BSWAP64(val);
}

/** @} */

#endif /* PLATFORM_UTILS_H */
