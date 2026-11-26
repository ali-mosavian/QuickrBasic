/* Depth: 1/z in 16 bits, as qgl's, stepped in 16.16 along a span. */
#ifndef GFX_DEPTH_H
#define GFX_DEPTH_H
#include <stdint.h>

/* Depth is 1/z by DEPTH_NEAR, as qgl's: nearer is larger, 0 is far. */
enum { DEPTH_NEAR = 65535 };

/* A target's depth: its first, and a row's step in depths. */
typedef struct {
    uint16_t *p;
    uint32_t row;
} Depth;

/* The depth at z, 16.16 (qgl's zacc): its integer half. */
static inline uint16_t depth_of(const int32_t z) { return (uint16_t)((uint32_t)z >> 16); }

/* z, n pixels on by dz, wrapping as qgl's adds. */
static inline int32_t depth_on(const int32_t z, const int32_t dz, const int32_t n)
{
    return (int32_t)((uint32_t)z + (uint32_t)dz * (uint32_t)n);
}

/* Whether depth z shows over there: qgl's test, which a depth there z or nearer fails. */
static inline bool depth_shows(const uint16_t there, const uint16_t z) { return there < z; }
#endif
