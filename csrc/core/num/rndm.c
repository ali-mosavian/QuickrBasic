#include "num/rndm.h"

Rndm rndm;

Sngl rndm_rnd(Sngl x)
{
    if (x << 1) {
        if (x & SNGL_SIGN) rndm.seed = (x + (x >> 24)) & 0xFFFFFF;
        rndm.seed = (rndm.seed * 16598013u + 12820163u) & 0xFFFFFF;
    }
    uint32_t s = rndm.seed;
    if (!s) return 0;
    int top = 31 - __builtin_clz(s); /* seed / 2^24, exactly */
    return ((s << (23 - top)) & 0x7FFFFF) | (uint32_t)(top + FBIAS - 24) << 23;
}
