/* SINGLE arithmetic's interface. How it works: fpmath.c. */
#ifndef NUM_FPMATH_H
#define NUM_FPMATH_H
#include "fault.h"
#include "single.h"

/* The x87 set up; with none, says so and exits (flt_finit). */
void fmth_init(void);
/* After an error: the x87's stack empty, its control word the default (ex_fclr). */
void fmth_clear(void);

static inline Sngl fmth_add(const Sngl a, const Sngl b)
{
    return sngl_bits(sngl_val(a) + sngl_val(b));
}
static inline Sngl fmth_sub(const Sngl a, const Sngl b)
{
    return sngl_bits(sngl_val(a) - sngl_val(b));
}
static inline Sngl fmth_mul(const Sngl a, const Sngl b)
{
    return sngl_bits(sngl_val(a) * sngl_val(b));
}
/* a / b; ?DIVISION BY ZERO when b is 0 or -0. */
Sngl fmth_div(Fault *f, Sngl a, Sngl b);
/* a ^ b, at 64 bits; a whole power by squaring. ?ILLEGAL QUANTITY a negative base to a fraction,
   0 to a negative power; ?OVERFLOW past 2^32767.5, infinity below it. */
Sngl fmth_pow(Fault *f, Sngl a, Sngl b);

static inline Sngl fmth_from_i16(const int16_t v) { return sngl_bits((float)v); }
static inline Sngl fmth_from_u16(const uint16_t v) { return sngl_bits((float)v); }
static inline Sngl fmth_from_i32(const int32_t v) { return sngl_bits((float)v); }
/* The exponent of a SINGLE whose significand is whole. */
enum { SNGL_POINT = FBIAS + SNGL_EXP_SHIFT };
enum : Sngl { SNGL_MIN_I32 = 0xCF000000 /* -2^31 */ };

static inline uint32_t fmth_exponent(const Sngl a) { return a >> SNGL_EXP_SHIFT & SNGL_EXP_MASK; }

/* fistp dword: to the nearest, a half to even; INT32_MIN past the range (the x87's
   indefinite). */
static inline int32_t fmth_rint32(const Sngl a)
{
    const uint32_t e = fmth_exponent(a);

    if (e == SNGL_EXP_MASK || e >= FBIAS + 31)
        return INT32_MIN;
    if (e < FBIAS - 1)
        return 0;

    const uint64_t m = (a & SNGL_MANT) | SNGL_IMPLICIT;
    uint64_t q;

    if (e >= SNGL_POINT)
        q = m << (e - SNGL_POINT);
    else {
        const uint32_t shift = SNGL_POINT - e;
        const uint64_t rem = m & ((1ull << shift) - 1);
        const uint64_t half = 1ull << (shift - 1);
        q = m >> shift;

        if (rem > half || (rem == half && (q & 1)))
            q++;
    }

    const bool neg = a & SNGL_SIGN;

    if (q > (neg ? 0x80000000u : (uint32_t)INT32_MAX))
        return INT32_MIN;

    return neg ? (int32_t)(0 - q) : (int32_t)q;
}

/* a rounded to the nearest, a half to even; ?ILLEGAL QUANTITY out of the type's range. */
int16_t fmth_to_i16(Fault *f, Sngl a);
uint16_t fmth_to_u16(Fault *f, Sngl a);
int32_t fmth_to_i32(Fault *f, Sngl a);
/* INT: floor(a). */
Sngl fmth_floor(Sngl a);
/* FIX: a toward 0 (declared for vm/builtins' sc_fix, which does it inline). */
Sngl fmth_fix(Sngl a);

/* The builtins of vm/exec's g_fntab. */
Sngl fmth_sqr(Fault *f, Sngl a); /* ?ILLEGAL QUANTITY under 0 */
Sngl fmth_exp(Fault *f, Sngl a); /* ?OVERFLOW from 2^32767.5; infinity below it */
Sngl fmth_log(Fault *f, Sngl a); /* ?ILLEGAL QUANTITY at 0 or under */
Sngl fmth_sin(Fault *f, Sngl a); /* SIN COS TAN: ?ILLEGAL QUANTITY past 2^31 */
Sngl fmth_cos(Fault *f, Sngl a);
Sngl fmth_tan(Fault *f, Sngl a);
Sngl fmth_atn(Fault *f, Sngl a);
#endif
