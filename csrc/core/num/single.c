/* SINGLE: compares, conversions and number text in.
 *
 * # How it works
 * A FLOAT is an IEEE single kept packed, a Sngl, as the asm's EAX: in a Cell's low dword (`.u`),
 * its high dword undefined. Every routine of the num group takes and returns a Sngl, never a C
 * float: llrm keeps a float result at the x87's precision until it is stored, and a Sngl is
 * stored, so each operation rounds once, as the asm's fstp. The arithmetic is fpmath.c's.
 *
 * # asm structure (src/float/flt.asm)
 * | asm | here | |
 * |---|---|---|
 * | err_ov | repl_error(E_OV) | MOVED: io/repl.c |
 * | flt_neg | inline `a ^ SNGL_SIGN` unless +-0 | D1 |
 * | flt_abs, flt_sgn, flt_norm, flt_w16 | sngl_abs, sngl_sgn, sngl_norm, sngl_w16 | MIRROR |
 * | flt_cmp, flt_key | sngl_cmp, order_key | MIRROR; static |
 * | flt_insg | sngl_skipsign | MIRROR |
 * | num_amp | sngl_hexoct | MIRROR |
 * | flt_in | sngl_in | MIRROR |
 * | flt_dv | sngl_from_dbl(dubl_in(...)) | MIRROR |
 * | flt_out | sngl_print | MIRROR |
 *
 * - D1. No function: callers inline it.
 */
#include "num/single.h"
#include "session.h"
#include "io/console.h"
#include "num/double.h"

enum : uint8_t { TOK_PLUS = 0xAA, TOK_MINUS = 0xAB }; /* comp/dfa.h's TK_PLUS, TK_MINUS */

Sngl sngl_abs(const Sngl a) { return a & ~SNGL_SIGN; }

int sngl_sgn(const Sngl a)
{
    if (!(a & ~SNGL_SIGN))
        return 0;

    return a & SNGL_SIGN ? -1 : 1;
}

Sngl sngl_norm(const uint32_t mag, const bool neg)
{
    const int top = 31 - __builtin_clz(mag);
    return ((mag << (SNGL_EXP_SHIFT - top)) & SNGL_MANT) |
           (uint32_t)(top + FBIAS) << SNGL_EXP_SHIFT | (neg ? SNGL_SIGN : 0);
}

bool sngl_w16(const Sngl a, int16_t *const out)
{
    const uint32_t e = a >> SNGL_EXP_SHIFT & SNGL_EXP_MASK;

    if (!e) {
        if (a & SNGL_MANT)
            return false; /* a denormal */
        *out = 0;
        return true;
    }

    if (e < FBIAS || e - FBIAS >= 15)
        return false;

    const uint32_t m = (a & SNGL_MANT) | SNGL_IMPLICIT;
    const uint32_t shift = SNGL_EXP_SHIFT - (e - FBIAS);

    if (m & ((1u << shift) - 1)) /* not whole */
        return false;

    const int16_t v = (int16_t)(m >> shift);
    *out = a & SNGL_SIGN ? (int16_t)-v : v;
    return true;
}

bool sngl_skipsign(const uint8_t **const p)
{
    while (**p == ' ')
        ++*p;

    const uint8_t c = **p;

    if (c == '-' || c == TOK_MINUS) {
        ++*p;
        return true;
    }

    if (c == '+' || c == TOK_PLUS)
        ++*p;

    return false;
}

/* c as a digit of base 1 << bits; -1 none. */
static int digit_of(const uint8_t c, const uint8_t bits)
{
    const uint8_t d = c - '0';

    if (d < 10)
        return d < 1u << bits ? d : -1;

    const uint8_t x = upcase(c) - 'A';
    return bits == 4 && x < 6 ? x + 10 : -1;
}

bool sngl_hexoct(const uint8_t **const p, uint32_t *const val)
{
    const uint8_t base = upcase(**p);
    const uint8_t bits = base == 'H' ? 4 : base == 'O' ? 3 : 0;

    if (!bits)
        return false;

    const uint8_t *s = *p + 1;
    uint32_t v = 0;

    for (int d; (d = digit_of(*s, bits)) >= 0; s++)
        v = v << bits | (uint32_t)d;

    *p = s;
    *val = v;
    return true;
}

Sngl sngl_from_dbl(const double d) { return sngl_bits((float)d); }

Sngl sngl_in(Fault *const f, const uint8_t **const p)
{
    return sngl_from_dbl(dubl_in(f, p, nullptr));
}

void sngl_print(Cons *const co, const Sngl a)
{
    uint8_t t[DUBL_TXT];
    const uint32_t n = dubl_fmtf(a, t);

    for (uint32_t i = 0; i < n; i++)
        cons_putc(co, t[i]);

    cons_space(co);
}
