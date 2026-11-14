/* The x87's functions on an x86-64 host (Rosetta's x87 off x86).
 *
 * # How it works
 * DOS32's pfpu.asm in C over long double, the x87's own instructions where C has none. Control
 * words and order of operations are the asm's, so results match it wherever the host's x87
 * matches a real one.
 */
#include "plat/pfpu.h"

#pragma STDC FENV_ACCESS ON /* no FP operation moves across control() */

typedef long double X;

enum : uint16_t {
    CW_DEF = 0x027F, /* between calls: 53 bits, to nearest */
    CW_24 = 0x007F,  /* FLOATs' */
    CW_64 = 0x037F,
    CW_DOWN = 0x047F,
};

static constexpr float BIG = 2147483648.0f;  /* 2^31 */
static constexpr float EXP_LIMIT = 32767.5f; /* in powers of 2 */
static constexpr double DMAX = 1.7976931348623157E308;
static const double p10[23] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                               1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                               1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

[[gnu::noinline]]
static void control(const uint16_t cw)
{
    __asm__ volatile("fldcw %0" ::"m"(cw) : "memory");
}

static X rndint(X x)
{
    __asm__ volatile("frndint" : "+t"(x));
    return x;
}
static X x2m1(X x)
{
    __asm__ volatile("f2xm1" : "+t"(x));
    return x;
}
static X scale2(X x, const X e)
{
    __asm__ volatile("fscale" : "+t"(x) : "u"(e));
    return x;
}
static X yl2x(const X y, const X x) /* y * log2 x */
{
    X r;
    __asm__ volatile("fyl2x" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}
static X sine(X x)
{
    __asm__ volatile("fsin" : "+t"(x));
    return x;
}
static X cosine(X x)
{
    __asm__ volatile("fcos" : "+t"(x));
    return x;
}
static X tangent(X x)
{
    __asm__ volatile("fptan\n\tfstp %%st(0)" : "+t"(x));
    return x;
}
static X arctan(const X x)
{
    X r;
    __asm__ volatile("fpatan" : "=t"(r) : "0"((X)1), "u"(x) : "st(1)");
    return r;
}
static X log2e(void)
{
    X r;
    __asm__ volatile("fldl2e" : "=t"(r));
    return r;
}
static X ln2(void)
{
    X r;
    __asm__ volatile("fldln2" : "=t"(r));
    return r;
}
static X lg2(void)
{
    X r;
    __asm__ volatile("fldlg2" : "=t"(r));
    return r;
}
static int32_t to_int32(const X x) /* fistp at the current rounding */
{
    int32_t i;
    __asm__ volatile("fistpl %0" : "=m"(i) : "t"(x) : "st");
    return i;
}
static int64_t to_int64(const X x)
{
    int64_t i;
    __asm__ volatile("fistpll %0" : "=m"(i) : "t"(x) : "st");
    return i;
}
static X root(X x)
{
    __asm__ volatile("fsqrt" : "+t"(x));
    return x;
}
static X fabsx(const X x) { return x < 0 ? -x : x; }
static bool is_nan(const X x) { return x != x; }

/* A function's result, or its error. */
typedef struct {
    X v;
    uint8_t err;
} R;

static R ok(const X v) { return (R){v, PFPU_OK}; }
static R fail(const uint8_t err) { return (R){0, err}; }

static X pow2(const X t) /* fx_x2x */
{
    const X i = rndint(t);
    return scale2(x2m1(t - i) + 1, i);
}

static R fx_exp(const X x)
{
    const X t = x * log2e();

    if (t >= EXP_LIMIT)
        return fail(PFPU_OV);

    return ok(pow2(t));
}

static R fx_log(const X x)
{
    if (!(x > 0))
        return fail(PFPU_IQ);

    return ok(yl2x(ln2(), x));
}

static bool trig_range(const X x) { return !(fabsx(x) > BIG); }

static R fx_sin(const X x) { return trig_range(x) ? ok(sine(x)) : fail(PFPU_IQ); }
static R fx_cos(const X x) { return trig_range(x) ? ok(cosine(x)) : fail(PFPU_IQ); }
static R fx_tan(const X x) { return trig_range(x) ? ok(tangent(x)) : fail(PFPU_IQ); }
static R fx_atn(const X x) { return ok(arctan(x)); }

static R f_sqr(const X x) /* FLOAT: Illegal quantity under 0 (NaN too: ftst unordered) */
{
    if (!(x >= 0))
        return fail(PFPU_IQ);

    return ok(root(x));
}
static R d_sqr(const X x) { return ok(root(x)); } /* DOUBLE: NaN under 0 */

/* An infinite result is Overflow only when b log2 |a| reaches 2^32767.5. */
static R checked(const X r, const X a, const X b)
{
    if (fabsx(r) > DMAX && yl2x(b, fabsx(a)) >= EXP_LIMIT)
        return fail(PFPU_OV);

    return ok(r);
}

static R fx_pow(const X a, const X b)
{
    const bool whole = is_nan(b) || rndint(b) == b; /* fcomp unordered: equal */
    if (whole && !(fabsx(b) >= BIG)) {
        const int32_t i = to_int32(b);
        uint32_t n = i < 0 ? 0u - (uint32_t)i : (uint32_t)i;
        X r = 1, m = a;

        for (; n; n >>= 1) {
            if (n & 1)
                r *= m;

            m *= m;
        }

        if (i < 0) {
            if (r == 0 || is_nan(r)) /* ftst: zero or unordered */
                return fail(PFPU_IQ);

            r = 1 / r;
        }

        return checked(r, a, b);
    }

    if (!(a >= 0))
        return fail(PFPU_IQ);
    if (a == 0)
        return b > 0 ? checked(0, a, b) : fail(PFPU_IQ);

    return checked(pow2(yl2x(b, a)), a, b);
}

void pfpu_init(void) { control(CW_DEF); }
void pfpu_reset(void) { control(CW_DEF); }

static R by_function(const uint8_t f, const X x, const bool dbl)
{
    switch (f) {
    case PF_SQR:
        return dbl ? d_sqr(x) : f_sqr(x);

    case PF_SIN:
        return fx_sin(x);

    case PF_COS:
        return fx_cos(x);

    case PF_TAN:
        return fx_tan(x);

    case PF_ATN:
        return fx_atn(x);

    case PF_LOG:
        return fx_log(x);

    default:
        return fx_exp(x);
    }
}

static float as_float(const uint32_t bits)
{
    float f;
    __builtin_memcpy(&f, &bits, sizeof f);
    return f;
}
static uint32_t float_bits(const float f)
{
    uint32_t bits;
    __builtin_memcpy(&bits, &f, sizeof bits);
    return bits;
}

uint8_t pfpu_fn(const uint8_t f, const uint32_t x, uint32_t *const out)
{
    control(f == PF_EXP ? CW_64 : CW_24);
    const R r = by_function(f, as_float(x), false);

    if (!r.err)
        *out = float_bits((float)r.v);

    control(CW_DEF);
    return r.err;
}

uint8_t pfpu_dfn(const uint8_t f, const double x, double *const out)
{
    control(CW_64);
    const R r = by_function(f, x, true);

    if (!r.err)
        *out = (double)r.v;

    control(CW_DEF);
    return r.err;
}

uint8_t pfpu_pow(const uint32_t a, const uint32_t b, uint32_t *const out)
{
    control(CW_64);
    const R r = fx_pow(as_float(a), as_float(b));

    if (!r.err)
        *out = float_bits((float)r.v);

    control(CW_DEF);
    return r.err;
}

uint8_t pfpu_dpow(const double a, const double b, double *const out)
{
    control(CW_64);
    const R r = fx_pow(a, b);

    if (!r.err)
        *out = (double)r.v;

    control(CW_DEF);
    return r.err;
}

/* x * 10^p by the exact powers to 10^22 (dbl_scale). */
static X scale10(X x, int32_t p)
{
    for (; p > 22; p -= 22)
        x *= p10[22];
    for (; p < -22; p += 22)
        x /= p10[22];

    return p >= 0 ? x * p10[p] : x / p10[-p];
}

void pfpu_scale10(const uint64_t m, const int32_t p, double *const out)
{
    control(CW_DEF);
    *out = (double)scale10((X)(int64_t)m, p);
}

uint64_t pfpu_digits(const double v, const uint32_t ndig, int32_t *const dp)
{
    control(CW_64);
    const X x = v;
    const X lg = yl2x(lg2(), x);
    control(CW_DOWN);
    int32_t p = to_int32(lg);
    control(CW_64);
    X n;

    for (uint32_t tries = 4;; p--) {
        n = scale10(x, (int32_t)ndig - 1 - p);

        if (!--tries || n >= p10[ndig - 1])
            break;
    }

    n = rndint(n);

    if (n >= p10[ndig]) {
        n = p10[ndig - 1];
        p++;
    }

    const uint64_t digits = (uint64_t)to_int64(n);
    *dp = p;
    control(CW_DEF);
    return digits;
}
