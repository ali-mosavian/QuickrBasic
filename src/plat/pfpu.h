/* The x87's functions: what C cannot say (FSIN, FYL2X, F2XM1, FPTAN, FPATAN, FSCALE, FRNDINT,
 * FBSTP, 80-bit intermediates, the control word).
 *
 * # How it works
 * Each function is the asm's instructions, so results match it bit for bit, under DOSBox's x87
 * too. On DOS32 it is a jwasm file, cdecl.
 *
 * The control word between calls is 027Fh: 53-bit precision, to nearest, exceptions masked. So
 * C's double + - * / are IEEE double's, as the asm's D ops at its 53 bits, and C's float ops
 * round once to 53 bits and once to 24 on the store, which for + - * / and sqrt is the single
 * rounding the asm's 24-bit control word gives (53 >= 2*24+2). Each function sets the precision
 * and rounding its asm routine used and puts 027Fh back.
 *
 * A FLOAT goes in and out as its bits (num/single.h's Sngl), a result stored by fstp straight from
 * the 80-bit register, so it rounds once. A function that can fail returns PFPU_OK or the error;
 * it never leaves anything on the x87's stack.
 */
#ifndef PLAT_PFPU_H
#define PLAT_PFPU_H
#include <cdecl.h>
#include "base.h"

enum : uint8_t { PFPU_OK, PFPU_IQ, PFPU_OV }; /* ?ILLEGAL QUANTITY, ?OVERFLOW */

/* The functions of pfpu_fn and pfpu_dfn. */
enum : uint8_t { PF_SQR, PF_SIN, PF_COS, PF_TAN, PF_ATN, PF_LOG, PF_EXP };

/* The x87 found, its control word the default; with none, prints "A math coprocessor is
   required." and exits 1 (flt_finit). */
CDECL void pfpu_init(void);
/* fninit and the default control word (ex_fclr). */
CDECL void pfpu_reset(void);

/* FLOAT f(x) into *out, as fpu.asm's fn_*: SQR, LOG, SIN .. ATN at 24 bits, EXP at 64.
   PFPU_IQ: SQR under 0, LOG at 0 or under, SIN COS TAN past 2^31. PFPU_OV: EXP when x log2 e
   reaches 32767.5. */
CDECL uint8_t pfpu_fn(uint8_t f, uint32_t x, uint32_t *out);
/* DOUBLE f(x) into *out, as D.FN's, all at 64 bits; SQR under 0 NaN, not an error. */
CDECL uint8_t pfpu_dfn(uint8_t f, double x, double *out);

/* a ^ b at 64 bits, a whole b under 2^31 by squaring (fx_pow). PFPU_IQ: a under 0 to any other
   b, 0 to a negative b; PFPU_OV: past the largest double when b log2 |a| reaches 32767.5. */
CDECL uint8_t pfpu_pow(uint32_t a, uint32_t b, uint32_t *out);
CDECL uint8_t pfpu_dpow(double a, double b, double *out);

/* *out = m * 10^p at 53 bits with the x87's exponent range, by the exact powers to 10^22: one
   rounding while |p| <= 22, the last the store's (dbl_in's dbl_scale). m under 2^63. */
CDECL void pfpu_scale10(uint64_t m, int32_t p, double *out);

/* fistp: x to the nearest whole, a half to even; past the range or a NaN, the indefinite (INT32_MIN,
   INT64_MIN). Inline, one instruction: the x87's on DOS32 (the control word's rounding), SSE's on
   the host (MXCSR's, to nearest too), the same results. */
static inline int32_t pfpu_rint32(const float x)
{
    int32_t v;
#if defined(__i386__)
    __asm__("fistpl %0" : "=m"(v) : "t"(x) : "st");
#elif defined(__x86_64__)
    __asm__("cvtss2si %1, %0" : "=r"(v) : "x"(x));
#else
    const float r = __builtin_rintf(x); /* past the range or a NaN: the indefinite, as the x87's */
    v = r >= -2147483648.0f && r < 2147483648.0f ? (int32_t)r : INT32_MIN;
#endif
    return v;
}

static inline int64_t pfpu_rint64(const double x)
{
    int64_t v;
#if defined(__i386__)
    __asm__("fistpll %0" : "=m"(v) : "t"(x) : "st");
#elif defined(__x86_64__)
    __asm__("cvtsd2si %1, %0" : "=r"(v) : "x"(x));
#else
    const double r = __builtin_rint(x);
    v = r >= -9223372036854775808.0 && r < 9223372036854775808.0 ? (int64_t)r : INT64_MIN;
#endif
    return v;
}

/* v (over 0, finite) as an ndig-digit integer, rounded once at 64 bits: v ~ N * 10^(*dp-ndig+1),
   *dp the first digit's power of ten; returns N, 10^(ndig-1) .. 10^ndig-1 (dbl_digits). */
CDECL uint64_t pfpu_digits(double v, uint32_t ndig, int32_t *dp);
#endif
