#include "num/rndm.h"
#include "session.h"

/* QBasic's generator: seed' = seed * MUL + ADD, mod 2^24. */
enum : uint32_t { RND_MUL = 16598013, RND_ADD = 12820163, RND_MASK = 0xFFFFFF, RND_BITS = 24 };


Sngl rndm_rnd(Session *const S, const Sngl x)
{
    if (x << 1) { /* not 0 */
        if (x & SNGL_SIGN)
            S->rndm.seed = (x + (x >> RND_BITS)) & RND_MASK;
        S->rndm.seed = (S->rndm.seed * RND_MUL + RND_ADD) & RND_MASK;
    }
    const uint32_t s = S->rndm.seed;
    if (!s)
        return 0;
    const int top = 31 - __builtin_clz(s); /* seed / 2^24, exactly */
    return ((s << (SNGL_EXP_SHIFT - top)) & SNGL_MANT) | (uint32_t)(top + FBIAS - RND_BITS) << SNGL_EXP_SHIFT;
}
