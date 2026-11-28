/* Levels: a texture's mipmap (levels.h). */
#include "core/levels.h"

#include "core/mem.h"
#include "core/palette.h"

uint32_t levels_of(const int32_t w, const int32_t h)
{
    const int32_t shorter = w < h ? w : h;
    uint32_t n = 1;

    while (n < LEVELS_MOST && shorter >> n >= LEVELS_LEAST)
        n++;

    return n;
}

/* The texel of format f that is the mean of n colours summed by channel, a, r, g and b; keyed,
   the key under two. A drawn texel never packs to the key: at 8 bits it is matched past index 0,
   at 15 and 16 black is the smallest blue. */
static uint32_t mean_of(
    const PxFormat f,
    const uint32_t *const sums,
    const uint32_t n,
    const bool keyed,
    const uint32_t *const row
)
{
    if (keyed && n < 2)
        return 0;

    uint32_t c = 0;

    for (uint32_t k = 0; k < 4; k++)
        c |= (sums[k] + n / 2) / n << (24 - 8 * k);

    if (f == PX_8)
        return keyed ? pal_nearest_drawn(row, c & PX_WHITE) : pal_nearest(row, c & PX_WHITE);

    const uint32_t v = px_pack(f, c);
    return keyed && px_key(f, v) ? 1 : v;
}

void levels_halve(
    const Pixels *const from,
    const Pixels *const into,
    const bool keyed,
    const uint32_t *const row
)
{
    const PxFormat f = from->format;
    const uint32_t b = px_bytes(f);

    for (int32_t y = 0; y < into->h; y++)
        for (int32_t x = 0; x < into->w; x++) {
            uint32_t sums[4] = {0};
            uint32_t n = 0;

            for (uint32_t k = 0; k < 4; k++) {
                const uint8_t *const p = from->p +
                                         (uint32_t)(2 * y + (int32_t)(k >> 1)) * from->pitch +
                                         (uint32_t)(2 * x + (int32_t)(k & 1)) * b;
                const uint32_t v = px_load(p, b);

                if (keyed && px_key(f, v))
                    continue;

                const uint32_t c = f == PX_8 ? PX_OPAQUE | row[v] : px_argb(f, v);

                for (uint32_t i = 0; i < 4; i++)
                    sums[i] += px_channel(c, 24 - 8 * i);

                n++;
            }

            px_store(
                into->p + (uint32_t)y * into->pitch + (uint32_t)x * b,
                mean_of(f, sums, n, keyed, row),
                b
            );
        }
}

bool levels_make(
    Levels *const l,
    const Pixels *const source,
    const bool keyed,
    const uint32_t *const row
)
{
    const uint32_t n = levels_of(source->w, source->h), b = px_bytes(source->format);
    uint32_t bytes = 0;

    for (uint32_t k = 1; k < n; k++)
        bytes += (uint32_t)(source->w >> k) * (uint32_t)(source->h >> k) * b;

    *l = (Levels){.at[0] = *source, .n = 1};

    if (n > 1 && !(l->block = mem_alloc(bytes)))
        return false;

    uint8_t *p = l->block;

    for (uint32_t k = 1; k < n; k++) {
        const int32_t w = source->w >> k, h = source->h >> k;

        l->at[k] = (Pixels){p, (uint32_t)w * b, w, h, source->format};
        levels_halve(&l->at[k - 1], &l->at[k], keyed, row);
        p += (uint32_t)(w * h) * b;
    }

    l->n = n;
    return true;
}

void levels_done(Levels *const l)
{
    mem_free(l->block);
    *l = (Levels){0};
}
