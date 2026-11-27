/* 1 / x, without the divide where x is 1, as a 2D draw's w is: an x87 divide costs a 486 73
   cycles. Told by the bits, as clang folds a float compare with 1 away, 1 / 1 being 1. */
#ifndef LIB_RECIPROCAL_H
#define LIB_RECIPROCAL_H
#include <stdint.h>

static inline float reciprocal(const float x)
{
    uint32_t bits;

    __builtin_memcpy(&bits, &x, sizeof bits);
    return bits == 0x3F800000u ? 1 : 1 / x;
}
#endif
