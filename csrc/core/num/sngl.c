#include "num/sngl.h"
#include "io/cons.h"
#include "num/dubl.h"

enum : uint8_t { TOK_PLUS = 0xAA, TOK_MINUS = 0xAB }; /* comp/dfa.h's TK_PLUS, TK_MINUS */

Sngl sngl_abs(Sngl a) { return a & ~SNGL_SIGN; }

static int32_t key(Sngl a)
{
    int32_t k = (int32_t)(a & ~SNGL_SIGN);
    return a & SNGL_SIGN ? -k : k;
}

int sngl_cmp(Sngl a, Sngl b)
{
    int32_t ka = key(a), kb = key(b);
    return ka == kb ? 0 : ka > kb ? 1 : -1;
}

int sngl_sgn(Sngl a)
{
    if (!(a & ~SNGL_SIGN)) return 0;
    return a & SNGL_SIGN ? -1 : 1;
}

Sngl sngl_norm(uint32_t mag, bool neg)
{
    int top = 31 - __builtin_clz(mag);
    return ((mag << (23 - top)) & 0x7FFFFF) | (uint32_t)(top + FBIAS) << 23 | (neg ? SNGL_SIGN : 0);
}

bool sngl_w16(Sngl a, int16_t *out)
{
    uint32_t e = a >> 23 & 0xFF;
    if (!e) {
        if (a & 0x7FFFFF) return false; /* a denormal */
        *out = 0;
        return true;
    }
    if (e < FBIAS || e - FBIAS >= 15) return false;
    uint32_t m = (a & 0x7FFFFF) | 0x800000, sh = 23 - (e - FBIAS);
    if (m & ((1u << sh) - 1)) return false;
    int16_t v = (int16_t)(m >> sh);
    *out = a & SNGL_SIGN ? (int16_t)-v : v;
    return true;
}

bool sngl_skipsign(const uint8_t **p)
{
    while (**p == ' ') ++*p;
    uint8_t c = **p;
    if (c == '-' || c == TOK_MINUS) {
        ++*p;
        return true;
    }
    if (c == '+' || c == TOK_PLUS) ++*p;
    return false;
}

static uint8_t upper(uint8_t c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }

bool sngl_hexoct(const uint8_t **p, uint32_t *val)
{
    const uint8_t *s = *p;
    uint8_t c = upper(*s), bits;
    if (c == 'H') bits = 4;
    else if (c == 'O') bits = 3;
    else return false;
    uint32_t v = 0;
    for (++s;; ++s) {
        c = upper(*s);
        uint8_t d = (uint8_t)(c - '0');
        if (d >= 10) {
            if (bits != 4) break;
            d = (uint8_t)(c - 'A');
            if (d >= 6) break;
            d += 10;
        }
        if (bits == 3 && d >= 8) break;
        v = v << bits | d;
    }
    *p = s;
    *val = v;
    return true;
}

Sngl sngl_from_dbl(double d)
{
    float f = (float)d;
    Sngl r;
    memcpy(&r, &f, 4);
    return r;
}

Sngl sngl_in(const uint8_t **p) { return sngl_from_dbl(dubl_in(p, nullptr)); }

void sngl_print(Sngl a)
{
    uint8_t t[DUBL_TXT];
    uint32_t n = dubl_fmtf(a, t);
    for (uint32_t i = 0; i < n; i++) cons_putc(t[i]);
    cons_space();
}
