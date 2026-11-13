#include "num/rndm.h"

/* QBasic's generator: seed' = seed * MUL + ADD, mod 2^24. */
enum : uint32_t { RND_MUL = 16598013, RND_ADD = 12820163, RND_MASK = 0xFFFFFF, RND_BITS = 24 };

Rndm rndm;

Sngl rndm_rnd(const Sngl x)
{
    if (x << 1) { /* not 0 */
        if (x & SNGL_SIGN)
            rndm.seed = (x + (x >> RND_BITS)) & RND_MASK;
        rndm.seed = (rndm.seed * RND_MUL + RND_ADD) & RND_MASK;
    }
    const uint32_t s = rndm.seed;
    if (!s)
        return 0;
    const int top = 31 - __builtin_clz(s); /* seed / 2^24, exactly */
    return ((s << (SNGL_EXP_SHIFT - top)) & SNGL_MANT) | (uint32_t)(top + FBIAS - RND_BITS) << SNGL_EXP_SHIFT;
}
