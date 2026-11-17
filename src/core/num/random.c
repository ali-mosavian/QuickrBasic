/* RND: QBasic's 24-bit generator.
 *
 * # How it works
 * seed' = seed * MUL + ADD, mod 2^24. RND(x): 0 repeats the last; a negative x seeds from its bits
 * first; then the next. The first seed, RNDM_SEED0, is set at start (boot).
 *
 * # asm structure (retired/asm32/float/ftr.asm)
 * | asm | here | |
 * |---|---|---|
 * | g_state.ft.rs | Rndm.seed | MIRROR |
 * | fn_rnd | rndm_rnd | MIRROR |
 * | flt_pi, g_kpi | SNGL_PI | MOVED: single.h |
 * | g_kone | SNGL_ONE | MOVED: single.h |
 */
#include "num/random.h"

/* QBasic's generator: seed' = seed * MUL + ADD, mod 2^24. */
enum : uint32_t { RND_MUL = 16598013, RND_ADD = 12820163, RND_MASK = 0xFFFFFF, RND_BITS = 24 };

Sngl rndm_rnd(Rndm *const r, const Sngl x)
{
    if (x << 1) { /* not 0 */
        if (x & SNGL_SIGN)
            r->seed = (x + (x >> RND_BITS)) & RND_MASK;

        r->seed = (r->seed * RND_MUL + RND_ADD) & RND_MASK;
    }

    const uint32_t s = r->seed;

    if (!s)
        return 0;

    const int top = 31 - __builtin_clz(s); /* seed / 2^24, exactly */
    return ((s << (SNGL_EXP_SHIFT - top)) & SNGL_MANT) | (uint32_t)(top + FBIAS - RND_BITS)
                                                             << SNGL_EXP_SHIFT;
}
