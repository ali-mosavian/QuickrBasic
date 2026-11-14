/* A D op's result is stored to its Cell before it is looked at: llrm keeps a double at the x87's
   precision until stored, and the store is the asm's fstp qword. */
#include "num/double.h"
#include "session.h"
#include "io/console.h"
#include "io/repl.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "plat/pfpu.h"
#include "vm/exec.h"

/* An IEEE double's fields. */
enum : uint64_t {
    DBL_SIGN = 1ull << 63,
    DBL_MANT = (1ull << 52) - 1,
    DBL_IMPLICIT = 1ull << 52,
    DBL_MINUS_ONE = 0xBFF0000000000000,
};
enum {
    DBL_MANT_BITS = 52,
    DBL_EXP_MASK = 0x7FF,
    DBL_BIAS = 1023,
    DBL_DIGITS = 16,
    SNGL_DIGITS = 7
};

/* Mantissa digits taken at most: under 2^63. */
enum { MANT_DIGITS_MAX = 18, EXPONENT_MAX = 10000 };

static uint64_t bits(const double d) { return (Cell){.d = d}.raw; }
static double val(const uint64_t u) { return (Cell){.raw = u}.d; }

static uint32_t exponent(const uint64_t u) { return u >> DBL_MANT_BITS & DBL_EXP_MASK; }
static bool special(const uint64_t u) { return exponent(u) == DBL_EXP_MASK; } /* infinity or NaN */
static bool is_zero(const uint64_t u) { return !(u << 1); }

/* Overflow past the largest. */
static void check(Session *const S, const Cell *const c)
{
    if (special(c->raw))
        repl_error(S, E_OV);
}

static void status(Session *const S, const uint8_t st)
{
    if (st == PFPU_IQ)
        repl_error(S, E_IQ);
    if (st == PFPU_OV)
        repl_error(S, E_OV);
}

/* ---- text in -------------------------------------------------------------------------------- */

static void skip(const uint8_t **const p)
{
    while (**p == ' ')
        ++*p;
}

/* The next digit at *p, spaces skipped, *p past it; -1 none. */
static int next_digit(const uint8_t **const p)
{
    skip(p);
    const uint8_t d = **p - '0';

    if (d > 9)
        return -1;

    ++*p;
    return d;
}

/* A number's digits: up to MANT_DIGITS_MAX of them, the power of ten for the rest. */
typedef struct {
    uint64_t mant;
    int32_t pow10;
    uint32_t digits; /* the digits taken */
    uint32_t zeros;  /* the zeros after the point before them */
} Mantissa;

static Mantissa read_mantissa(const uint8_t **const p)
{
    Mantissa m = {};

    bool point = false;

    for (;;) {
        skip(p);

        if (**p == '.') {
            if (point)
                break;

            point = true;
            ++*p;
            continue;
        }

        const int d = next_digit(p);

        if (d < 0)
            break;
        if (!m.digits && !d) { /* a leading zero */
            if (point) {
                m.pow10--;
                m.zeros++;
            }

            continue;
        }

        if (m.digits >= MANT_DIGITS_MAX) { /* past it, their power only */
            if (!point)
                m.pow10++;

            continue;
        }

        m.digits++;
        m.mant = m.mant * 10 + (uint32_t)d;

        if (point)
            m.pow10--;
    }

    return m;
}

/* An exponent after D or E: its value; *kind DK_DBL for D. */
static int32_t read_exponent(const uint8_t **const p, uint8_t *const kind)
{
    skip(p);
    const uint8_t c = upcase(**p);

    if (c != 'D' && c != 'E')
        return 0;
    if (c == 'D')
        *kind = DK_DBL;

    ++*p;
    const bool neg = sngl_skipsign(p);
    int32_t e = 0;

    for (int d; (d = next_digit(p)) >= 0;)
        if (e < EXPONENT_MAX)
            e = e * 10 + d;

    return neg ? -e : e;
}

double dubl_in(Session *const S, const uint8_t **const p, uint8_t *const kind)
{
    skip(p);

    if (**p == '&') {
        uint32_t v;
        ++*p;

        if (sngl_hexoct(p, &v)) {
            if (kind)
                *kind = DK_ANY;

            return (double)(int32_t)v;
        }

        --*p;
    }

    const bool neg = sngl_skipsign(p);
    Mantissa m = read_mantissa(p);
    uint8_t k = DK_ANY;
    m.pow10 += read_exponent(p, &k);
    skip(p);

    if (**p == '#') {
        ++*p;
        k = DK_DBL;
    } else if (**p == '!') {
        ++*p;
        k = DK_SNGL;
    }

    if (!k && m.digits + m.zeros > SNGL_DIGITS) /* BC's count: the zeros after the point too */
        k = DK_DBL;

    Cell r;
    pfpu_scale10(m.mant, m.pow10, &r.d);

    if (neg)
        r.raw ^= DBL_SIGN;

    check(S, &r);

    if (kind)
        *kind = k;

    return r.d;
}

/* ---- text out ------------------------------------------------------------------------------- */

uint32_t dubl_digits(const double v, const uint32_t ndig, uint8_t *const dig, int32_t *const dp)
{
    uint64_t num = pfpu_digits(v, ndig, dp);

    for (uint32_t i = ndig; i-- > 0; num /= 10)
        dig[i] = (uint8_t)('0' + num % 10);

    uint32_t n = ndig;

    while (n > 1 && dig[n - 1] == '0')
        n--;

    return n;
}

/* v as PRINT spells it with ndig digits at most and exponent letter ex. */
static uint32_t fmt(const double v, const uint32_t ndig, const uint8_t ex, uint8_t *const out)
{
    uint8_t *o = out;
    *o++ = ' ';

    if (!(v < 0 || v > 0)) { /* 0, and NaN as ftst sees it */
        *o++ = '0';
        return (uint32_t)(o - out);
    }

    if (v < 0)
        out[0] = '-';

    const uint64_t u = bits(v) & ~DBL_SIGN;

    if (special(u)) {
        static const char inf[] = "1.#INF";
        memcpy(o, inf, strlen(inf));
        return (uint32_t)(o + strlen(inf) - out);
    }

    uint8_t dig[DBL_DIGITS];
    int32_t dp;
    const uint32_t n = dubl_digits(val(u), ndig, dig, &dp);

    if (dp >= 0 && dp < (int32_t)ndig) { /* the whole part, then the rest after a point */
        uint32_t b = 0;

        for (; b <= (uint32_t)dp; b++)
            *o++ = b < n ? dig[b] : '0';
        if (b < n) {
            *o++ = '.';

            for (; b < n; b++)
                *o++ = dig[b];
        }

        return (uint32_t)(o - out);
    }

    if (dp < 0 && n + (uint32_t)-dp - 1 <= ndig) { /* under 1: .000ddd */
        *o++ = '.';

        for (int32_t z = -dp - 1; z > 0; z--)
            *o++ = '0';
        for (uint32_t b = 0; b < n; b++)
            *o++ = dig[b];

        return (uint32_t)(o - out);
    }

    *o++ = dig[0]; /* d.dddD+nn */
    if (n > 1) {
        *o++ = '.';

        for (uint32_t b = 1; b < n; b++)
            *o++ = dig[b];
    }

    *o++ = ex;
    *o++ = dp < 0 ? '-' : '+';
    uint32_t e = dp < 0 ? (uint32_t)-dp : (uint32_t)dp;

    if (e >= 100) {
        *o++ = (uint8_t)('0' + e / 100);
        e %= 100;
    }

    *o++ = (uint8_t)('0' + e / 10);
    *o++ = (uint8_t)('0' + e % 10);
    return (uint32_t)(o - out);
}

uint32_t dubl_fmt(const double v, uint8_t *const out) { return fmt(v, DBL_DIGITS, 'D', out); }

uint32_t dubl_fmtf(const Sngl v, uint8_t *const out)
{
    return fmt(sngl_val(v), SNGL_DIGITS, 'E', out);
}

/* ---- loads and stores ----------------------------------------------------------------------- */

static Cell cell_at(const Addr a) { return load_cell(at(a)); }
static void cell_put(const Addr a, const Cell c) { store_cell(at(a), c); }

void dubl_ldv(Session *const S)
{
    exec_push(&S->vm, cell_at(symb_cell(&S->symb, exec_u16(&S->vm))));
}
void dubl_stv(Session *const S)
{
    const Addr cell = symb_cell(&S->symb, exec_u16(&S->vm));
    cell_put(cell, exec_pop(&S->vm));
}

void dubl_ldk(Session *const S)
{
    exec_push(&S->vm, load_cell(S->vm.ip));
    S->vm.ip += sizeof(Cell);
}

void dubl_lda1(Session *const S)
{
    const int32_t i = exec_popi(&S->vm);
    exec_push(&S->vm, cell_at(exec_elem1(S, exec_u16(&S->vm), i)));
}

void dubl_lda(Session *const S) { exec_push(&S->vm, cell_at(exec_elem(S))); }

void dubl_sta1(Session *const S)
{
    const Cell d = exec_pop(&S->vm);
    const int32_t i = exec_popi(&S->vm);
    cell_put(exec_elem1(S, exec_u16(&S->vm), i), d);
}

void dubl_sta(Session *const S)
{
    const Cell d = exec_pop(&S->vm);
    cell_put(exec_elem(S), d);
}

/* ---- arithmetic ----------------------------------------------------------------------------- */

void dubl_add(Session *const S)
{
    const double b = exec_popd(&S->vm);
    S->vm.sp->d += b;
    check(S, S->vm.sp);
}

void dubl_sub(Session *const S)
{
    const double b = exec_popd(&S->vm);
    S->vm.sp->d -= b;
    check(S, S->vm.sp);
}

void dubl_mul(Session *const S)
{
    const double b = exec_popd(&S->vm);
    S->vm.sp->d *= b;
    check(S, S->vm.sp);
}

/* Division by zero: the divisor 0. */
void dubl_div(Session *const S)
{
    if (is_zero(S->vm.sp->raw))
        repl_error(S, E_DZ);

    const double b = exec_popd(&S->vm);
    S->vm.sp->d /= b;
    check(S, S->vm.sp);
}

void dubl_neg(Session *const S)
{
    if (!is_zero(S->vm.sp->raw))
        S->vm.sp->raw ^= DBL_SIGN;
}

void dubl_cmp(Session *const S)
{
    const double b = exec_popd(&S->vm);
    const double a = S->vm.sp->d;
    const uint8_t rel = a > b ? DC_GT : a < b ? DC_LT : DC_EQ;
    S->vm.sp->u = rel & exec_u8(&S->vm) ? UINT32_MAX : 0;
}

void dubl_nz(Session *const S) { S->vm.sp->u = is_zero(S->vm.sp->raw) ? 0 : UINT32_MAX; }
void dubl_itod(Session *const S) { S->vm.sp->d = S->vm.sp->i; }
void dubl_ftod(Session *const S) { S->vm.sp->d = S->vm.sp->f; }
void dubl_tof(Session *const S) { S->vm.sp->u = sngl_from_dbl(S->vm.sp->d); }

/* fistp qword: to the nearest, a half to even; INT64_MIN past the range (the indefinite). */
static int64_t round_to_int64(const uint64_t u)
{
    const uint32_t e = exponent(u);

    if (e >= DBL_BIAS + 63)
        return INT64_MIN; /* -2^63 itself too */
    if (e < DBL_BIAS - 1)
        return 0;

    const uint64_t m = (u & DBL_MANT) | DBL_IMPLICIT;
    const uint32_t point = DBL_BIAS + DBL_MANT_BITS; /* the exponent of a whole significand */
    uint64_t q;

    if (e >= point)
        q = m << (e - point);
    else {
        const uint32_t shift = point - e;
        const uint64_t rem = m & ((1ull << shift) - 1);
        const uint64_t half = 1ull << (shift - 1);
        q = m >> shift;

        if (rem > half || (rem == half && (q & 1)))
            q++;
    }

    return u & DBL_SIGN ? (int64_t)(0 - q) : (int64_t)q;
}

/* Overflow past an INT. */
void dubl_toi(Session *const S)
{
    const int64_t v = round_to_int64(S->vm.sp->raw);
    S->vm.sp->raw = (uint64_t)v;

    if (v != (int32_t)v)
        repl_error(S, E_OV);
}

/* frndint, down or toward 0. */
static uint64_t round_whole(uint64_t u, const bool down)
{
    const uint32_t e = exponent(u);
    const uint64_t sign = u & DBL_SIGN;

    if (e >= DBL_BIAS + DBL_MANT_BITS) /* whole already */
        return u;
    if (e < DBL_BIAS) { /* under 1 */
        if (down && sign && !is_zero(u))
            return DBL_MINUS_ONE;

        return sign;
    }

    const uint64_t frac = (1ull << (DBL_BIAS + DBL_MANT_BITS - e)) - 1;

    if (!(u & frac))
        return u;
    if (down && sign)
        u += frac + 1;

    return u & ~frac;
}

void dubl_fn(Session *const S)
{
    const uint8_t k = exec_u8(&S->vm);
    Cell *const c = S->vm.sp;

    switch (k) {
    case DF_ABS:
        c->raw &= ~DBL_SIGN;
        break;

    case DF_SGN:
        c->i = c->d > 0 ? 1 : c->d < 0 ? -1 : 0;
        return; /* NaN 0 */
    case DF_INT:
        c->raw = round_whole(c->raw, true);
        break;

    case DF_FIX:
        c->raw = round_whole(c->raw, false);
        break;

    case DF_SQR:
        status(S, pfpu_dfn(PF_SQR, c->d, &c->d));
        return;

    case DF_EXP:
        status(S, pfpu_dfn(PF_EXP, c->d, &c->d));
        return;

    default: {
        static const uint8_t pf[] = {
            [DF_SIN] = PF_SIN,
            [DF_COS] = PF_COS,
            [DF_TAN] = PF_TAN,
            [DF_ATN] = PF_ATN,
            [DF_LOG] = PF_LOG
        };

        status(S, pfpu_dfn(pf[k], c->d, &c->d));
    }
    }

    check(S, c);
}

void dubl_pow(Session *const S)
{
    const double b = exec_popd(&S->vm);
    status(S, pfpu_dpow(S->vm.sp->d, b, &S->vm.sp->d));
}

void dubl_print(Session *const S)
{
    uint8_t t[DUBL_TXT];
    const uint32_t n = dubl_fmt(exec_popd(&S->vm), t);

    for (uint32_t i = 0; i < n; i++)
        cons_putc(S, t[i]);

    cons_space(S);
}

void dubl_str(Session *const S)
{
    const uint32_t n = dubl_fmt(exec_popd(&S->vm), S->cons.sbuf);
    exec_pusha(&S->vm, strg_buftmp(S, (uint16_t)n));
}
