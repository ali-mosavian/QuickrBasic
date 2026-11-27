/* INDEX textures on a card that holds colours (indices.h). */
#include "plat/dos32/accel/indices.h"

enum : uint32_t { RGB = 0xFFFFFF, BYTE = 0xFF, PER = 0x0101 };

/* A 565 word's channels as R, G and B, each its top bits repeated below. */
static uint32_t widened(const uint32_t v)
{
    const uint32_t r = v >> 11, g = v >> 5 & 0x3F, b = v & 0x1F;

    return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
}

/* &HRRGGBB as the card's 565 word holds it. */
static uint32_t word_of(const uint32_t rgb)
{
    return (rgb >> 19 & 0x1F) << 11 | (rgb >> 10 & 0x3F) << 5 | (rgb >> 3 & 0x1F);
}

static uint32_t word_read(const uint8_t *const rgba)
{
    return word_of((uint32_t)rgba[0] << 16 | (uint32_t)rgba[1] << 8 | rgba[2]);
}

uint32_t indices_code(const uint32_t j)
{
    const uint32_t v = (j & BYTE) * PER;

    return (v >> 11) << 19 | (v >> 5 & 0x3F) << 10 | (v & 0x1F) << 3;
}

uint8_t indices_of_code(const uint8_t *const rgba) { return (uint8_t)word_read(rgba); }

uint32_t indices_colour(
    const uint32_t argb,
    const uint32_t colours[INDICES],
    const IndicesHeld held
)
{
    const uint32_t j = argb >> 16 & BYTE, alpha = argb & ~RGB;

    return held == HELD_COLOURS ? argb
           : held == HELD_SHOWN ? alpha | (colours[j] & RGB)
                                : alpha | indices_code(j);
}

uint8_t indices_nearest(
    const uint8_t *const rgba,
    const uint32_t colours[INDICES],
    uint16_t *const memo
)
{
    const uint32_t word = word_read(rgba);

    if (memo[word])
        return (uint8_t)(memo[word] - 1);

    const uint32_t c = widened(word);
    uint32_t best = 0, least = UINT32_MAX;

    for (uint32_t i = 0; i < INDICES && least; i++) {
        const uint32_t k = widened(word_of(colours[i]));
        const int32_t dr = (int32_t)(k >> 16) - (int32_t)(c >> 16);
        const int32_t dg = (int32_t)(k >> 8 & BYTE) - (int32_t)(c >> 8 & BYTE);
        const int32_t db = (int32_t)(k & BYTE) - (int32_t)(c & BYTE);
        const uint32_t d = (uint32_t)(dr * dr + dg * dg + db * db);

        best = d < least ? i : best;
        least = d < least ? d : least;
    }

    memo[word] = (uint16_t)(best + 1);
    return (uint8_t)best;
}
