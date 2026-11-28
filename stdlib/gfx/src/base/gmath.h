/* The arithmetic a slice has no C library for: sine and cosine, a square root, floor.
 *
 * # How it works
 * The angle is reduced to a quarter turn's distance from the nearest multiple of pi/2, pi/2 taken
 * in two parts (Cody and Waite) so the reduction loses no bits, then Cephes' sinf and cosf
 * polynomials. The square root is the machine's instruction: the slices are built without errno.
 */
#ifndef GFX_GMATH_H
#define GFX_GMATH_H
#include <stdint.h>

#define GM_PI 3.14159265358979f

static inline float gm_sqrt(const float x) { return __builtin_sqrtf(x); }

/* 1 / x, without the divide where x is 1, as a 2D draw's z is: an x87 divide costs a 486 73
   cycles. Told by the bits, as clang folds a float compare with 1 away, 1 / 1 being 1. */
static inline float gm_reciprocal(const float x)
{
    uint32_t bits;

    __builtin_memcpy(&bits, &x, sizeof bits);
    return bits == 0x3F800000u ? 1 : 1 / x;
}

static inline int32_t gm_floor(const float x)
{
    const int32_t i = (int32_t)x;
    return (float)i > x ? i - 1 : i;
}

/* The sine and cosine of a, radians, to *s and *c. An a of 0, an unturned draw's, told by its
   bits: the series' own 0 and 1, without its 300 cycles on a 486. */
static inline void gm_sincos(const float a, float *const s, float *const c)
{
    uint32_t bits;

    __builtin_memcpy(&bits, &a, sizeof bits);

    if (!(bits << 1)) {
        *s = a, *c = 1;
        return;
    }

    const float n = a * (2 / GM_PI);
    const int32_t q = gm_floor(n + 0.5f);
    const float r = a - (float)q * 1.5707963705062866f + (float)q * 4.371139000186241e-8f;
    const float r2 = r * r;
    const float sin_r =
        r + r * r2 * (-1.6666654611e-1f + r2 * (8.3321608736e-3f + r2 * -1.9515295891e-4f));
    const float cos_r =
        1 - 0.5f * r2 +
        r2 * r2 *
            (4.166664568298827e-2f + r2 * (-1.388731625493765e-3f + r2 * 2.443315711809948e-5f));

    switch (q & 3) {
    case 0:
        *s = sin_r, *c = cos_r;
        return;

    case 1:
        *s = cos_r, *c = -sin_r;
        return;

    case 2:
        *s = -sin_r, *c = -cos_r;
        return;

    default:
        *s = -cos_r, *c = sin_r;
        return;
    }
}
#endif
