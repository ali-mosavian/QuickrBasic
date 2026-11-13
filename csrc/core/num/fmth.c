/* + - * / in C round as the asm's: a result goes out as a Sngl, stored by fstp dword, and the
   x87 at 53 bits (plat/pfpu.h) rounds + - * / once more harmlessly. Rounding to an integer is
   bit work, exact. */
#include "num/fmth.h"
#include "io/repl.h"
#include "plat/pfpu.h"

static float val(Sngl a)
{
    float f;
    memcpy(&f, &a, 4);
    return f;
}

static Sngl bits(float f)
{
    Sngl a;
    memcpy(&a, &f, 4);
    return a;
}

void fmth_init(void) { pfpu_init(); }
void fmth_clear(void) { pfpu_reset(); }

Sngl fmth_add(Sngl a, Sngl b) { return bits(val(a) + val(b)); }
Sngl fmth_sub(Sngl a, Sngl b) { return bits(val(a) - val(b)); }
Sngl fmth_mul(Sngl a, Sngl b) { return bits(val(a) * val(b)); }

Sngl fmth_div(Sngl a, Sngl b)
{
    if (!(b & ~SNGL_SIGN)) repl_error(E_DZ);
    return bits(val(a) / val(b));
}

static Sngl check(uint8_t st, Sngl r)
{
    if (st == PFPU_IQ) repl_error(E_IQ);
    if (st == PFPU_OV) repl_error(E_OV);
    return r;
}

Sngl fmth_pow(Sngl a, Sngl b)
{
    Sngl r;
    return check(pfpu_pow(a, b, &r), r);
}

Sngl fmth_from_i16(int16_t v) { return bits((float)v); }
Sngl fmth_from_u16(uint16_t v) { return bits((float)v); }
Sngl fmth_from_i32(int32_t v) { return bits((float)v); }

/* fistp dword at the nearest, a half to even: 80000000h past the range (the x87's
   indefinite). */
static int32_t fistp(Sngl a)
{
    uint32_t e = a >> 23 & 0xFF;
    if (e == 0xFF || e >= FBIAS + 31) return INT32_MIN;
    if (e < FBIAS - 1) return 0;
    uint64_t m = (a & 0x7FFFFF) | 0x800000, q;
    if (e >= 150) q = m << (e - 150);
    else {
        uint32_t sh = 150 - e;
        uint64_t rem = m & ((1ull << sh) - 1), half = 1ull << (sh - 1);
        q = m >> sh;
        if (rem > half || (rem == half && (q & 1))) q++;
    }
    if (q > (a & SNGL_SIGN ? 0x80000000u : 0x7FFFFFFFu)) return INT32_MIN;
    return a & SNGL_SIGN ? (int32_t)(0 - q) : (int32_t)q;
}

int16_t fmth_to_i16(Sngl a)
{
    int32_t v = fistp(a);
    if (v != (int16_t)v) repl_error(E_IQ);
    return (int16_t)v;
}

uint16_t fmth_to_u16(Sngl a)
{
    int32_t v = fistp(a);
    if ((uint32_t)v > 0xFFFF) repl_error(E_IQ);
    return (uint16_t)v;
}

int32_t fmth_to_i32(Sngl a)
{
    int32_t v = fistp(a);
    if (v == INT32_MIN && a != 0xCF000000) repl_error(E_IQ); /* -2^31 itself is one */
    return v;
}

/* frndint toward 0 (down false) or down. */
static Sngl rndint(Sngl a, bool down)
{
    uint32_t e = a >> 23 & 0xFF, neg = a & SNGL_SIGN;
    if (e >= 150) return a;
    if (e < FBIAS) {
        if (down && neg && (a & ~SNGL_SIGN)) return 0xBF800000; /* -1 */
        return neg;
    }
    uint32_t frac = (1u << (150 - e)) - 1;
    if (!(a & frac)) return a;
    if (down && neg) a += frac + 1;
    return a & ~frac;
}

Sngl fmth_floor(Sngl a) { return rndint(a, true); }
Sngl fmth_fix(Sngl a) { return rndint(a, false); }

static Sngl fn(uint8_t f, Sngl a)
{
    Sngl r;
    return check(pfpu_fn(f, a, &r), r);
}

Sngl fmth_sqr(Sngl a) { return fn(PF_SQR, a); }
Sngl fmth_exp(Sngl a) { return fn(PF_EXP, a); }
Sngl fmth_log(Sngl a) { return fn(PF_LOG, a); }
Sngl fmth_sin(Sngl a) { return fn(PF_SIN, a); }
Sngl fmth_cos(Sngl a) { return fn(PF_COS, a); }
Sngl fmth_tan(Sngl a) { return fn(PF_TAN, a); }
Sngl fmth_atn(Sngl a) { return fn(PF_ATN, a); }
