/* A D op's result is stored to its Cell before it is looked at: llrm keeps a double at the x87's
   precision until stored, and the store is the asm's fstp qword. */
#include "num/dubl.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "plat/pfpu.h"
#include "vm/exec.h"

static uint64_t bits(double d)
{
    uint64_t u;
    memcpy(&u, &d, 8);
    return u;
}

static double val(uint64_t u)
{
    double d;
    memcpy(&d, &u, 8);
    return d;
}

static bool special(uint64_t u) { return (u >> 52 & 0x7FF) == 0x7FF; } /* infinity or NaN */
static bool is_zero(uint64_t u) { return !(u << 1); }

/* dbl_chk: ?OVERFLOW past the largest. */
static void check(const Cell *c)
{
    if (special(c->raw)) repl_error(E_OV);
}

static void status(uint8_t st)
{
    if (st == PFPU_IQ) repl_error(E_IQ);
    if (st == PFPU_OV) repl_error(E_OV);
}

static void skip(const uint8_t **p)
{
    while (**p == ' ') ++*p;
}

double dubl_in(const uint8_t **p, uint8_t *kind)
{
    uint8_t k = DK_ANY;
    skip(p);
    if (**p == '&') {
        uint32_t v;
        ++*p;
        if (sngl_hexoct(p, &v)) {
            if (kind) *kind = DK_ANY;
            return (double)(int32_t)v;
        }
        --*p;
    }
    bool neg = sngl_skipsign(p), point = false;
    uint64_t m = 0;
    uint32_t n = 0, zeros = 0; /* the digits taken; the zeros after the point before them */
    int32_t pw = 0;            /* the power of ten */
    for (;;) {
        skip(p);
        uint8_t c = **p;
        if (c == '.') {
            if (point) break;
            point = true;
            ++*p;
            continue;
        }
        uint8_t d = (uint8_t)(c - '0');
        if (d > 9) break;
        ++*p;
        if (!n && !d) { /* a leading zero */
            if (point) pw--, zeros++;
            continue;
        }
        if (n >= 18) { /* under 2^63: past it their power only */
            if (!point) pw++;
            continue;
        }
        n++;
        m = m * 10 + d;
        if (point) pw--;
    }
    skip(p);
    uint8_t c = **p & 0xDF;
    if (c == 'D' || c == 'E') {
        if (c == 'D') k = DK_DBL;
        ++*p;
        bool eneg = sngl_skipsign(p);
        int32_t e = 0;
        for (;;) {
            skip(p);
            uint8_t d = (uint8_t)(**p - '0');
            if (d > 9) break;
            ++*p;
            if (e < 10000) e = e * 10 + d;
        }
        pw += eneg ? -e : e;
    }
    skip(p);
    if (**p == '#') {
        ++*p;
        k = DK_DBL;
    } else if (**p == '!') {
        ++*p;
        k = DK_SNGL;
    }
    if (!k && n + zeros > 7) k = DK_DBL; /* BC's count: the zeros after the point too */
    Cell r;
    pfpu_scale10(m, pw, &r.d);
    if (neg) r.raw ^= 1ull << 63;
    check(&r);
    if (kind) *kind = k;
    return r.d;
}

uint32_t dubl_digits(double v, uint32_t ndig, uint8_t *dig, int32_t *dp)
{
    uint64_t num = pfpu_digits(v, ndig, dp);
    for (uint32_t i = ndig; i--; num /= 10) dig[i] = (uint8_t)('0' + num % 10);
    uint32_t n = ndig;
    while (n > 1 && dig[n - 1] == '0') n--;
    return n;
}

/* num_fmt: v as PRINT spells it with ndig digits at most and exponent letter ex. */
static uint32_t fmt(double v, uint32_t ndig, uint8_t ex, uint8_t *out)
{
    uint8_t *o = out, dig[16];
    *o++ = ' ';
    if (!(v < 0 || v > 0)) { /* 0, and NaN as ftst sees it */
        *o++ = '0';
        return (uint32_t)(o - out);
    }
    uint64_t u = bits(v);
    if (v < 0) out[0] = '-', u &= ~(1ull << 63);
    if (special(u)) {
        memcpy(o, "1.#INF", 6);
        return (uint32_t)(o + 6 - out);
    }
    int32_t dp;
    uint32_t n = dubl_digits(val(u), ndig, dig, &dp), b = 0;
    if (dp < (int32_t)ndig) {
        if (dp >= 0) { /* the whole part */
            for (; b <= (uint32_t)dp; b++) *o++ = b < n ? dig[b] : '0';
            if (b < n) {
                *o++ = '.';
                while (b < n) *o++ = dig[b++];
            }
            return (uint32_t)(o - out);
        }
        if (n + (uint32_t)-dp - 1 <= ndig) { /* under 1: .000ddd */
            *o++ = '.';
            for (int32_t z = -dp - 1; z > 0; z--) *o++ = '0';
            while (b < n) *o++ = dig[b++];
            return (uint32_t)(o - out);
        }
    }
    *o++ = dig[0]; /* d.dddD+nn */
    if (n > 1) {
        *o++ = '.';
        for (b = 1; b < n; b++) *o++ = dig[b];
    }
    *o++ = ex;
    *o++ = dp < 0 ? '-' : '+';
    uint32_t e = dp < 0 ? (uint32_t)-dp : (uint32_t)dp;
    if (e >= 100) *o++ = (uint8_t)('0' + e / 100), e %= 100;
    *o++ = (uint8_t)('0' + e / 10);
    *o++ = (uint8_t)('0' + e % 10);
    return (uint32_t)(o - out);
}

uint32_t dubl_fmt(double v, uint8_t *out) { return fmt(v, 16, 'D', out); }

uint32_t dubl_fmtf(Sngl v, uint8_t *out)
{
    float f;
    memcpy(&f, &v, 4);
    return fmt((double)f, 7, 'E', out);
}

static Cell rdcell(Addr a)
{
    Cell c;
    memcpy(&c, at(a), 8);
    return c;
}

static void wrcell(Addr a, Cell c) { memcpy(at(a), &c, 8); }

void dubl_ldv(void) { exec_push(rdcell(symb_cell(exec_u16()))); }
void dubl_stv(void) { wrcell(symb_cell(exec_u16()), exec_pop()); }

void dubl_ldk(void)
{
    Cell c;
    memcpy(&c, exec.ip, 8);
    exec.ip += 8;
    exec_push(c);
}

void dubl_lda1(void)
{
    int32_t i = exec_popi();
    exec_push(rdcell(exec_elem1(exec_u16(), i)));
}

void dubl_lda(void) { exec_push(rdcell(exec_elem())); }

void dubl_sta1(void)
{
    Cell d = exec_pop();
    int32_t i = exec_popi();
    wrcell(exec_elem1(exec_u16(), i), d);
}

void dubl_sta(void)
{
    Cell d = exec_pop();
    wrcell(exec_elem(), d);
}

void dubl_add(void)
{
    double b = exec_popd();
    exec.sp->d += b;
    check(exec.sp);
}

void dubl_sub(void)
{
    double b = exec_popd();
    exec.sp->d -= b;
    check(exec.sp);
}

void dubl_mul(void)
{
    double b = exec_popd();
    exec.sp->d *= b;
    check(exec.sp);
}

void dubl_div(void)
{
    if (is_zero(exec.sp->raw)) repl_error(E_DZ);
    double b = exec_popd();
    exec.sp->d /= b;
    check(exec.sp);
}

void dubl_neg(void)
{
    if (!is_zero(exec.sp->raw)) exec.sp->raw ^= 1ull << 63;
}

void dubl_cmp(void)
{
    double b = exec_popd(), a = exec.sp->d;
    uint8_t rel = a > b ? DC_GT : a < b ? DC_LT : DC_EQ;
    exec.sp->u = rel & exec_u8() ? 0xFFFFFFFF : 0;
}

void dubl_nz(void) { exec.sp->u = is_zero(exec.sp->raw) ? 0 : 0xFFFFFFFF; }
void dubl_itod(void) { exec.sp->d = exec.sp->i; }
void dubl_ftod(void) { exec.sp->d = exec.sp->f; }
void dubl_tof(void) { exec.sp->u = sngl_from_dbl(exec.sp->d); }

/* fistp qword at the nearest, a half to even: INT64_MIN past the range (the indefinite). */
static int64_t fistp(uint64_t u)
{
    uint32_t e = u >> 52 & 0x7FF;
    bool neg = u >> 63;
    if (e >= 1023 + 63) return INT64_MIN; /* -2^63 itself too */
    if (e < 1022) return 0;
    uint64_t m = (u & 0xFFFFFFFFFFFFF) | 1ull << 52, q;
    if (e >= 1075) q = m << (e - 1075);
    else {
        uint32_t sh = 1075 - e;
        uint64_t rem = m & ((1ull << sh) - 1), half = 1ull << (sh - 1);
        q = m >> sh;
        if (rem > half || (rem == half && (q & 1))) q++;
    }
    return neg ? (int64_t)(0 - q) : (int64_t)q;
}

void dubl_toi(void)
{
    int64_t v = fistp(exec.sp->raw);
    exec.sp->raw = (uint64_t)v;
    if (v != (int32_t)v) repl_error(E_OV);
}

/* frndint down or toward 0. */
static uint64_t rndint(uint64_t u, bool down)
{
    uint32_t e = u >> 52 & 0x7FF;
    uint64_t neg = u & 1ull << 63;
    if (e >= 1075) return u;
    if (e < 1023) {
        if (down && neg && !is_zero(u)) return 0xBFF0000000000000; /* -1 */
        return neg;
    }
    uint64_t frac = (1ull << (1075 - e)) - 1;
    if (!(u & frac)) return u;
    if (down && neg) u += frac + 1;
    return u & ~frac;
}

void dubl_fn(void)
{
    uint8_t k = exec_u8();
    Cell *c = exec.sp;
    switch (k) {
    case DF_ABS: c->raw &= ~(1ull << 63); break;
    case DF_SGN: c->u = (uint32_t)(c->d > 0 ? 1 : c->d < 0 ? -1 : 0); return; /* NaN 0 */
    case DF_INT: c->raw = rndint(c->raw, true); break;
    case DF_FIX: c->raw = rndint(c->raw, false); break;
    case DF_SQR: status(pfpu_dfn(PF_SQR, c->d, &c->d)); return;
    case DF_EXP: status(pfpu_dfn(PF_EXP, c->d, &c->d)); return;
    default: {
        static const uint8_t pf[] = {[DF_SIN] = PF_SIN, [DF_COS] = PF_COS, [DF_TAN] = PF_TAN,
                                     [DF_ATN] = PF_ATN, [DF_LOG] = PF_LOG};
        status(pfpu_dfn(pf[k], c->d, &c->d));
    }
    }
    check(c);
}

void dubl_pow(void)
{
    double b = exec_popd();
    status(pfpu_dpow(exec.sp->d, b, &exec.sp->d));
}

void dubl_print(void)
{
    uint8_t t[DUBL_TXT];
    uint32_t n = dubl_fmt(exec_popd(), t);
    for (uint32_t i = 0; i < n; i++) cons_putc(t[i]);
    cons_space();
}

void dubl_str(void)
{
    uint32_t n = dubl_fmt(exec_popd(), cons.sbuf);
    exec_pusha(strg_buftmp((uint16_t)n));
}
