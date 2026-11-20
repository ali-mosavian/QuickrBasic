/* 64-bit division by shifts and subtractions, for a target whose compiler calls a helper for it.
 *
 * # How it works
 * Operands of 32 bits take the 32-bit division. Wider ones shift the dividend's bits into the
 * remainder, highest first, subtracting the divisor where it fits. Nothing here divides 64 bits,
 * so the helpers built on it (lib/crt.c) never call themselves (tools/libcalls_test.py).
 */
#include "lib/div64.h"

uint64_t div64_u(const uint64_t n, const uint64_t d, uint64_t *const rem)
{
    if (!(d >> 32) && (!(n >> 32) || !d)) {
        *rem = (uint32_t)n % (uint32_t)d;
        return (uint32_t)n / (uint32_t)d;
    }

    uint64_t q = 0, r = 0;

    for (uint32_t i = 64; i-- > 0;) {
        r = r << 1 | (n >> i & 1);

        if (r >= d) {
            r -= d;
            q |= (uint64_t)1 << i;
        }
    }

    *rem = r;
    return q;
}

int64_t div64_s(const int64_t n, const int64_t d, int64_t *const rem)
{
    uint64_t r;
    const uint64_t q =
        div64_u(n < 0 ? 0 - (uint64_t)n : (uint64_t)n, d < 0 ? 0 - (uint64_t)d : (uint64_t)d, &r);

    *rem = n < 0 ? (int64_t)(0 - r) : (int64_t)r;
    return (n < 0) != (d < 0) ? (int64_t)(0 - q) : (int64_t)q;
}
