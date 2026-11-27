/* 1 / x, without the divide where x is 1, as a 2D draw's w is: an x87 divide costs a 486 73
   cycles. clang folds the test away, 1 / 1 being 1, even told by the bits where they are known:
   the divide's x is hidden from it. */
#ifndef LIB_RECIPROCAL_H
#define LIB_RECIPROCAL_H
#include <stdint.h>

static inline float reciprocal(const float x)
{
    uint32_t bits;
    float y;

    __builtin_memcpy(&bits, &x, sizeof bits);

    if (bits == 0x3F800000u)
        return 1;

    __asm__("" : "+r"(bits));
    __builtin_memcpy(&y, &bits, sizeof y);
    return 1 / y;
}
#endif
