/* Texels as a card takes them (texels.h). */
#include "plat/dos32/accel/texels.h"
#include <stddef.h>

enum { KEYED = 0x8000, FIVE = 0x1F, HALF = 128, OPAQUE = 255, RGBA = 4 };

static uint32_t rounded_up(const uint32_t n)
{
    uint32_t p = 1;

    while (p < n)
        p <<= 1;

    return p;
}

CutGrid texels_grid(
    const uint16_t w,
    const uint16_t h,
    const bool indexed,
    const AccelCard *const c
)
{
    uint32_t tw = rounded_up(w), th = rounded_up(h);

    const uint32_t least = 1u << c->least,
                   across = indexed && least < TEXELS_INDEXED ? TEXELS_INDEXED : least;

    tw = tw > c->side ? c->side : tw < across ? across : tw;
    th = th > c->side ? c->side : th < least ? least : th;

    while (tw > th << c->aspect)
        th <<= 1;

    while (th > tw << c->aspect)
        tw <<= 1;

    return (CutGrid){
        .tile_w = (uint16_t)tw,
        .tile_h = (uint16_t)th,
        .across = (uint16_t)((w + tw - 1) / tw),
        .down = (uint16_t)((h + th - 1) / th),
    };
}

void texels_from_rgba(
    uint16_t *const to,
    const uint32_t to_pitch,
    const uint8_t *const from,
    const uint32_t from_pitch,
    const uint32_t w,
    const uint32_t h
)
{
    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *const in = from + (size_t)y * from_pitch;
        uint16_t *const out = to + (size_t)y * to_pitch;

        for (uint32_t x = 0; x < w; x++) {
            const uint8_t *const c = in + RGBA * x;
            out[x] = (uint16_t)((c[3] >= HALF ? KEYED : 0) | (c[0] >> 3) << 10 | (c[1] >> 3) << 5 |
                                c[2] >> 3);
        }
    }
}

/* A channel's five bits as eight, its top bits repeated below. */
static uint8_t widened(const uint32_t five) { return (uint8_t)(five << 3 | five >> 2); }

void texels_to_rgba(
    uint8_t *const to,
    const uint16_t *const from,
    const uint32_t from_pitch,
    const uint32_t w,
    const uint32_t h
)
{
    for (uint32_t y = 0; y < h; y++) {
        const uint16_t *const in = from + (size_t)y * from_pitch;
        uint8_t *const out = to + (size_t)y * w * RGBA;

        for (uint32_t x = 0; x < w; x++) {
            const uint32_t t = in[x];
            uint8_t *const c = out + RGBA * x;
            c[0] = widened(t >> 10 & FIVE);
            c[1] = widened(t >> 5 & FIVE);
            c[2] = widened(t & FIVE);
            c[3] = t & KEYED ? OPAQUE : 0;
        }
    }
}

uint32_t texels_reached(const uint32_t n, const uint32_t at, const uint32_t size)
{
    const uint32_t most = n - at + 1;

    return most < size ? most : size;
}

void texels_tile(
    const uint16_t *const texels,
    const uint16_t w,
    const uint16_t h,
    const CutGrid *const g,
    const uint32_t column,
    const uint32_t row,
    const uint32_t y0,
    const uint32_t y1,
    const uint32_t wide,
    uint16_t *const out
)
{
    const uint32_t left = column * g->tile_w, top = row * g->tile_h;

    for (uint32_t y = y0; y < y1; y++) {
        const uint32_t ty = top + y < h ? top + y : h - 1u;
        const uint16_t *const in = texels + (size_t)ty * w;
        uint16_t *const to = out + (size_t)(y - y0) * wide;

        for (uint32_t x = 0; x < wide; x++)
            to[x] = in[left + x < w ? left + x : w - 1u];
    }
}

void texels_from_indices(
    uint16_t *const to,
    const uint32_t to_pitch,
    const uint8_t *const from,
    const uint32_t from_pitch,
    const uint32_t w,
    const uint32_t h
)
{
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++)
            to[(size_t)y * to_pitch + x] = from[(size_t)y * from_pitch + x];
}

void texels_to_indices(
    uint8_t *const to,
    const uint16_t *const from,
    const uint32_t from_pitch,
    const uint32_t w,
    const uint32_t h
)
{
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++)
            for (uint32_t k = 0; k < RGBA; k++)
                to[((size_t)y * w + x) * RGBA + k] = (uint8_t)from[(size_t)y * from_pitch + x];
}
