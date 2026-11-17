/* math.h on wasm: what the CPU has as instructions, the rest JS's Math (wasm.mjs). */
#ifndef MATH_H
#define MATH_H
#define MATH_IMPORT(name) [[clang::import_module("math"), clang::import_name(#name)]]

MATH_IMPORT(sin) double sin(double x);
MATH_IMPORT(cos) double cos(double x);
MATH_IMPORT(tan) double tan(double x);
MATH_IMPORT(atan) double atan(double x);
MATH_IMPORT(atan2) double atan2(double y, double x);
MATH_IMPORT(exp) double exp(double x);
MATH_IMPORT(exp2) double exp2(double x);
MATH_IMPORT(log) double log(double x);
MATH_IMPORT(log2) double log2(double x);
MATH_IMPORT(log10) double log10(double x);
MATH_IMPORT(pow) double pow(double x, double y);
MATH_IMPORT(fmod) double fmod(double x, double y);

static inline double sqrt(const double x) { return __builtin_sqrt(x); }
static inline double fabs(const double x) { return __builtin_fabs(x); }
static inline double floor(const double x) { return __builtin_floor(x); }
static inline double ceil(const double x) { return __builtin_ceil(x); }
static inline double trunc(const double x) { return __builtin_trunc(x); }
static inline double rint(const double x) { return __builtin_rint(x); } /* f64.nearest: to even */
static inline double nearbyint(const double x) { return __builtin_rint(x); }
static inline long lrint(const double x) { return (long)__builtin_rint(x); }
static inline double round(const double x)
{
    return __builtin_trunc(x + __builtin_copysign(0.5, x));
}
static inline float sqrtf(const float x) { return __builtin_sqrtf(x); }
static inline float fabsf(const float x) { return __builtin_fabsf(x); }
static inline float floorf(const float x) { return __builtin_floorf(x); }

/* x * 2^e, in steps of 2^1000 so each power is a normal double. */
static inline double ldexp(double x, int e)
{
    for (; e > 1000; e -= 1000)
        x *= 0x1p1000;
    for (; e < -1000; e += 1000)
        x *= 0x1p-1000;

    const unsigned long long bits = (unsigned long long)(e + 1023) << 52;
    double m;
    __builtin_memcpy(&m, &bits, sizeof m);
    return x * m;
}

#define HUGE_VAL __builtin_huge_val()
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#define signbit(x) __builtin_signbit(x)
#endif
