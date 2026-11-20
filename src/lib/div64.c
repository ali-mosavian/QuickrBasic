/* 64-bit division on the CPU's divide, for a target whose compiler calls a helper for it.
 *
 * # How it works
 * x86-64 and arm64 divide 64 bits by 64. i386 divides 64 by 32 (DIV): div64_by32 builds 64 by 64
 * on it. A 32-bit divisor takes two, the high word first. A wider one makes a quotient of 32 bits:
 * the divisor normalized to its top bit, half the dividend divided by its high word, the estimate
 * one low at most, corrected once (Hacker's Delight, divdu). It builds on every target, its 64-by-32
 * divide each one's own, so a host's unit test runs it too. Nothing here divides 64 bits in C: the
 * helpers built on it (lib/crt.c) never call themselves (tools/libcalls_test.py).
 */
#include "lib/div64.h"

/* hi:lo / d, hi below d: the quotient, *r the remainder. d 0: the CPU's divide by zero. */
static inline uint32_t div_64by32(
    const uint32_t hi,
    const uint32_t lo,
    const uint32_t d,
    uint32_t *const r
)
{
#if defined(__i386__) || defined(__x86_64__)
    uint32_t q, rest;
    __asm__("divl %[d]" : "=a"(q), "=d"(rest) : "a"(lo), "d"(hi), [d] "rm"(d) : "cc");
    *r = rest;
    return q;
#elif defined(__aarch64__)
    uint64_t q, rest;
    div64_native((uint64_t)hi << 32 | lo, d, &q, &rest);
    *r = (uint32_t)rest;
    return (uint32_t)q;
#else
#error "lib/div64.c: no divide for this target"
#endif
}

uint64_t div64_by32(const uint64_t n, const uint64_t d, uint64_t *const rem)
{
    const uint32_t dh = (uint32_t)(d >> 32);

    if (!dh) {
        uint32_t r;
        const uint32_t qh = div_64by32(0, (uint32_t)(n >> 32), (uint32_t)d, &r);
        const uint32_t ql = div_64by32(r, (uint32_t)n, (uint32_t)d, &r);
        *rem = r;
        return (uint64_t)qh << 32 | ql;
    }

    const uint32_t s = (uint32_t)__builtin_clz(dh);
    const uint64_t half = n >> 1; /* its high word below the normalized divisor's */
    uint32_t r;
    const uint32_t est =
        div_64by32((uint32_t)(half >> 32), (uint32_t)half, (uint32_t)((d << s) >> 32), &r);
    uint64_t q = ((uint64_t)est << s) >> 31;

    if (q)
        q--;
    if (n - q * d >= d)
        q++;

    *rem = n - q * d;
    return q;
}

uint64_t div64_u(const uint64_t n, const uint64_t d, uint64_t *const rem)
{
#if defined(__i386__)
    return div64_by32(n, d, rem);
#else
    uint64_t q;
    div64_native(n, d, &q, rem);
    return q;
#endif
}

int64_t div64_s(const int64_t n, const int64_t d, int64_t *const rem)
{
    uint64_t r;
    const uint64_t q =
        div64_u(n < 0 ? 0 - (uint64_t)n : (uint64_t)n, d < 0 ? 0 - (uint64_t)d : (uint64_t)d, &r);

    *rem = n < 0 ? (int64_t)(0 - r) : (int64_t)r;
    return (n < 0) != (d < 0) ? (int64_t)(0 - q) : (int64_t)q;
}
