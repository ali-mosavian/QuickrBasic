/* 64-bit division on the CPU's divide, for lib/crt.c's helpers. How it works: div64.c. */
#ifndef LIB_DIV64_H
#define LIB_DIV64_H
#include <stdint.h>

/* n / d, *rem n % d. d 0: the CPU's divide by zero. */
uint64_t div64_u(uint64_t n, uint64_t d, uint64_t *rem);
/* n / d truncated, *rem n % d with n's sign. d 0: the CPU's divide by zero. */
int64_t div64_s(int64_t n, int64_t d, int64_t *rem);
/* div64_u on the 64-by-32 divide alone: i386's, any target's for its test. */
uint64_t div64_by32(uint64_t n, uint64_t d, uint64_t *rem);

#if defined(__x86_64__)
/* n / d to *q, n % d to *r: the CPU's 64-by-64 divide. */
static inline void div64_native(
    const uint64_t n,
    const uint64_t d,
    uint64_t *const q,
    uint64_t *const r
)
{
    __asm__("divq %[d]" : "=a"(*q), "=d"(*r) : "a"(n), "d"(0ull), [d] "rm"(d) : "cc");
}
#elif defined(__aarch64__)
static inline void div64_native(
    const uint64_t n,
    const uint64_t d,
    uint64_t *const q,
    uint64_t *const r
)
{
    uint64_t quot, rest;
    __asm__("udiv %[q], %[n], %[d]\n\tmsub %[r], %[q], %[d], %[n]"
            : [q] "=&r"(quot), [r] "=r"(rest)
            : [n] "r"(n), [d] "r"(d));
    *q = quot;
    *r = rest;
}
#endif
#endif
