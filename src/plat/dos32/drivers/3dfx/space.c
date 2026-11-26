/* A card's memory given out in ranges (space.h). */
#include "plat/dos32/drivers/3dfx/space.h"

uint32_t tdfx_take(TdfxSpace *const s, const uint32_t n, const uint32_t align)
{
    uint32_t from = 0;

    if (!n || s->count == TDFX_RANGES)
        return TDFX_NONE;

    for (uint32_t i = 0; i <= s->count; i++) {
        const uint32_t at = (from + align - 1) & ~(align - 1);
        const uint32_t end = i < s->count ? s->used[i].at : s->size;

        if (at <= end && n <= end - at) {
            for (uint32_t k = s->count; k > i; k--)
                s->used[k] = s->used[k - 1];

            s->used[i] = (TdfxRange){at, n};
            s->count++;
            return at;
        }

        if (i < s->count)
            from = s->used[i].at + s->used[i].n;
    }

    return TDFX_NONE;
}

void tdfx_give(TdfxSpace *const s, const uint32_t at)
{
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->used[i].at != at)
            continue;

        for (uint32_t k = i + 1; k < s->count; k++)
            s->used[k - 1] = s->used[k];

        s->count--;
        return;
    }
}
