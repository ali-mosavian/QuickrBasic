/* 8 bits a pixel's fillers (fill.h): an index a pixel, 0 the key. */
#include "core/mem.h"
#include "soft/fill.h"

static void span(uint8_t *const d, const int32_t n, const uint32_t colour)
{
    mem_fill(d, (uint8_t)colour, (size_t)n);
}

static void keyed(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    for (int32_t i = 0; i < n; i++)
        if (s[i])
            d[i] = s[i];
}

static void keyed_flipped(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    for (int32_t i = 0; i < n; i++)
        if (s[-i])
            d[i] = s[-i];
}

static void opaque(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    mem_copy(d, s, (size_t)n);
}

static void opaque_flipped(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    for (int32_t i = 0; i < n; i++)
        d[i] = s[-i];
}

static inline uint8_t next(const Pixels *const s, Steps *const st)
{
    return (uint8_t)steps_next(s, st, 1);
}

static void stepped_keyed(uint8_t *const d, const Pixels *const s, Steps st, const int32_t n)
{
    for (int32_t i = 0; i < n; i++) {
        const uint8_t texel = next(s, &st);

        if (texel)
            d[i] = texel;
    }
}

static void stepped_opaque(uint8_t *const d, const Pixels *const s, Steps st, const int32_t n)
{
    for (int32_t i = 0; i < n; i++)
        d[i] = next(s, &st);
}

static void fetch(uint32_t *const out, const Pixels *const s, Steps st, const int32_t n)
{
    for (int32_t i = 0; i < n; i++)
        out[i] = next(s, &st);
}

/* A texel's index: SOLID's, its own through the lut, or a direct one's colour shaded and
   matched. */
static uint32_t paint(const uint32_t texel, const Ink *const k)
{
    if (k->solid)
        return k->colour;

    if (k->source == PX_8)
        return k->lut ? k->lut[texel] : texel;

    return k->map[px_cell(px_shade(px_argb(k->source, texel), k->shade))];
}

static void ink(uint8_t *const d, const uint32_t *const texels, const int32_t n, const Ink *const k)
{
    for (int32_t i = 0; i < n; i++) {
        if (k->keyed && px_key(k->source, texels[i]))
            continue;

        const uint32_t c = paint(texels[i], k);
        d[i] = k->blend ? k->blend[c << 8 | d[i]] : (uint8_t)c;
    }
}

const Fill fill8 = {
    .span = span,
    .row =
        {
            [0] = keyed,
            [FILL_FLIPX] = keyed_flipped,
            [FILL_OPAQUE] = opaque,
            [FILL_OPAQUE | FILL_FLIPX] = opaque_flipped,
        },
    .step =
        {
            [0] = stepped_keyed,
            [FILL_OPAQUE] = stepped_opaque,
        },
    .fetch = fetch,
    .ink = ink,
    .paint = paint,
};
