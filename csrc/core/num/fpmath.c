/* SINGLE's arithmetic, conversions and functions.
 *
 * # How it works
 * + - * / and the conversions are C: a result goes out as a Sngl, stored by fstp dword, and the x87
 * at 53 bits (plat/pfpu.h) rounds + - * / once more harmlessly. Rounding to an integer is bit
 * work, exact. The functions and ^ are the x87's, through plat/pfpu.h. Past the range a result is
 * infinity, under it a denormal, as BC's.
 *
 * A binary op is a OP b: a the left operand (the asm's EDX, ARG), b the right (EAX, FAC). They share
 * one signature for vm/exec's g_bflt.
 *
 * # asm structure (src/float/fpu.asm)
 * | asm | here | |
 * |---|---|---|
 * | flt_finit, g_nofpu | fmth_init -> pfpu_init | MIRROR |
 * | ex_fclr | fmth_clear -> pfpu_reset | MIRROR |
 * | flt_add, flt_sub, flt_mul, flt_div | fmth_add, fmth_sub, fmth_mul, fmth_div | MIRROR; D1 |
 * | fn_pow | fmth_pow -> pfpu_pow | MIRROR |
 * | flt_from16, flt_fromu16, flt_from32 | fmth_from_i16, fmth_from_u16, fmth_from_i32 | MIRROR |
 * | flt_fix16, flt_fixu16, flt_fix32, flt_xfix | fmth_to_i16, fmth_to_u16, fmth_to_i32 | MIRROR |
 * | flt_int | fmth_floor | MIRROR |
 * | sc_fix's inline FRNDINT (vm/builtins) | fmth_fix | MOVED: here |
 * | fn_sqr, fn_exp, fn_log, fn_sin, fn_cos, fn_tan, fn_atn | fmth_sqr .. fmth_atn -> pfpu_fn(PF_*) | MIRROR |
 * | fx_exp, fx_x2x, fx_pow, fx_log, fx_sin, fx_cos, fx_tan, fx_trg, fx_atn | | MOVED: plat/pfpu.h |
 * | flt_fld, flt_fst, flt_xin, flt_xtst, flt_xfn | | D2 |
 * | g_fcw, g_fcwx, g_fcwd, g_dmax, g_pwa, g_pwb, g_pwi | | MOVED: plat/pfpu.h; D3 |
 * | g_state.ft.fpa, fpb, fpi, fpw | | D2 |
 *
 * - D1. fmth_add, fmth_sub and fmth_mul are inline in fpmath.h.
 * - D2. Gone: C does it, or pfpu does.
 * - D3. The default control word is 027Fh (53 bits), not 007Fh; see pfpu.h.
 */
#include "num/fpmath.h"
#include "session.h"
#include "io/repl.h"
#include "plat/pfpu.h"

void fmth_init(void) { pfpu_init(); }
void fmth_clear(void) { pfpu_reset(); }

/* The exponent of a SINGLE whose significand is whole. */
enum { SNGL_POINT = FBIAS + SNGL_EXP_SHIFT };
enum : Sngl { SNGL_MINUS_ONE = 0xBF800000, SNGL_MIN_I32 = 0xCF000000 /* -2^31 */ };

static uint32_t exponent(const Sngl a) { return a >> SNGL_EXP_SHIFT & SNGL_EXP_MASK; }

/* Division by zero. */
Sngl fmth_div(Session *const S, const Sngl a, const Sngl b)
{
    if (!(b & ~SNGL_SIGN))
        repl_error(S, E_DZ);

    return sngl_bits(sngl_val(a) / sngl_val(b));
}

/* r, after the status st of the routine that made it. */
static Sngl check(Session *const S, const uint8_t st, const Sngl r)
{
    if (st == PFPU_IQ)
        repl_error(S, E_IQ);
    if (st == PFPU_OV)
        repl_error(S, E_OV);

    return r;
}

Sngl fmth_pow(Session *const S, const Sngl a, const Sngl b)
{
    Sngl r;
    const uint8_t st = pfpu_pow(a, b, &r); /* before r is read */
    return check(S, st, r);
}

/* fistp dword: to the nearest, a half to even; INT32_MIN past the range (the x87's
   indefinite). */
static int32_t round_to_int32(const Sngl a)
{
    const uint32_t e = exponent(a);

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

/* Illegal function call outside -32768 to 32767. */
int16_t fmth_to_i16(Session *const S, const Sngl a)
{
    const int32_t v = round_to_int32(a);

    if (v != (int16_t)v)
        repl_error(S, E_IQ);

    return (int16_t)v;
}

/* Illegal function call outside 0 to 65535. */
uint16_t fmth_to_u16(Session *const S, const Sngl a)
{
    const int32_t v = round_to_int32(a);

    if ((uint32_t)v > UINT16_MAX)
        repl_error(S, E_IQ);

    return (uint16_t)v;
}

/* Illegal function call past a LONG. */
int32_t fmth_to_i32(Session *const S, const Sngl a)
{
    const int32_t v = round_to_int32(a);

    if (v == INT32_MIN && a != SNGL_MIN_I32) /* -2^31 itself is one */
        repl_error(S, E_IQ);

    return v;
}

/* frndint, down or toward 0. */
static Sngl round_whole(Sngl a, const bool down)
{
    const uint32_t e = exponent(a);
    const uint32_t sign = a & SNGL_SIGN;

    if (e >= SNGL_POINT) /* whole already */
        return a;
    if (e < FBIAS) { /* under 1 */
        if (down && sign && (a & ~SNGL_SIGN))
            return SNGL_MINUS_ONE;

        return sign;
    }

    const uint32_t frac = (1u << (SNGL_POINT - e)) - 1;

    if (!(a & frac))
        return a;
    if (down && sign)
        a += frac + 1;

    return a & ~frac;
}

Sngl fmth_floor(const Sngl a) { return round_whole(a, true); }
Sngl fmth_fix(const Sngl a) { return round_whole(a, false); }

static Sngl fn(Session *const S, const uint8_t f, const Sngl a)
{
    Sngl r;
    const uint8_t st = pfpu_fn(f, a, &r); /* before r is read */
    return check(S, st, r);
}

Sngl fmth_sqr(Session *const S, const Sngl a) { return fn(S, PF_SQR, a); }
Sngl fmth_exp(Session *const S, const Sngl a) { return fn(S, PF_EXP, a); }
Sngl fmth_log(Session *const S, const Sngl a) { return fn(S, PF_LOG, a); }
Sngl fmth_sin(Session *const S, const Sngl a) { return fn(S, PF_SIN, a); }
Sngl fmth_cos(Session *const S, const Sngl a) { return fn(S, PF_COS, a); }
Sngl fmth_tan(Session *const S, const Sngl a) { return fn(S, PF_TAN, a); }
Sngl fmth_atn(Session *const S, const Sngl a) { return fn(S, PF_ATN, a); }
