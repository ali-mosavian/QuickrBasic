/* lib/div64.c, the DOS build's 64-bit division helpers' (lib/crt.c), against this machine's. */
#include "lib/div64.h"
#include "unit.h"

/* div64_u's, the CPU's own here, and div64_by32's, i386's way. */
static bool unsigned_ok(const uint64_t n, const uint64_t d)
{
    uint64_t r, s;
    return !d || (div64_u(n, d, &r) == n / d && r == n % d && div64_by32(n, d, &s) == n / d && s == n % d);
}

/* Past INT64_MIN / -1 the machine's own is undefined. */
static bool signed_ok(const int64_t n, const int64_t d)
{
    int64_t r;
    return !d || (n == INT64_MIN && d == -1) || (div64_s(n, d, &r) == n / d && r == n % d);
}

TEST(div64_divides_as_the_machine)
{
    (void)S;
    static const uint64_t edges[] = {
        0,
        1,
        2,
        3,
        7,
        10,
        0xFFFFFFFFu,
        0x100000000u,
        0x100000001u,
        0x7FFFFFFFFFFFFFFFu,
        0x8000000000000000u,
        UINT64_MAX
    };
    const uint32_t ne = sizeof edges / sizeof *edges;

    for (uint32_t i = 0; i < ne; i++)
        for (uint32_t j = 0; j < ne; j++) {
            CHECK(unsigned_ok(edges[i], edges[j]));
            CHECK(signed_ok((int64_t)edges[i], (int64_t)edges[j]));
            CHECK(signed_ok(-(int64_t)(edges[i] >> 1), (int64_t)edges[j]));
        }

    uint64_t x = 88172645463325252u;

    for (uint32_t k = 0; k < 20000; k++) { /* xorshift: operands of every width */
        x ^= x << 13, x ^= x >> 7, x ^= x << 17;
        const uint64_t n = x >> (k % 64);
        x ^= x << 13, x ^= x >> 7, x ^= x << 17;
        const uint64_t d = x >> (k / 64 % 64);
        CHECK(unsigned_ok(n, d));
        CHECK(signed_ok((int64_t)n, (int64_t)d));
        CHECK(signed_ok((int64_t)n >> 3, -(int64_t)(d >> 2)));
    }
}
