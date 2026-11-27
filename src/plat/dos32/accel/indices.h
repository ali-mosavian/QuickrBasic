/* INDEX textures on a card that holds colours (qx/qxlist.h's minor 6): what a target holds for an
 * index, and the palette an INDEX texel is drawn through into it.
 *
 * # How it works
 * An RGBA target holds colours: an index texel draws as LOOKUP's word, a colour. The INDEX texture
 * shown holds COLOURS' colours: a texel draws as the colour of LOOKUP's index, and is read back as
 * the index nearest its colour. Any other INDEX target holds codes: index j as the 565 word j times
 * 0x0101, its low byte j back exactly, as no channel is dithered or mixed. Into an INDEX target a
 * colour's red is an index.
 */
#ifndef PLAT_DOS32_ACCEL_INDICES_H
#define PLAT_DOS32_ACCEL_INDICES_H
#include <stdbool.h>
#include <stdint.h>

enum { INDICES = 256, INDICES_CELLS = 1 << 16, INDICES_RGB = 0xFFFFFF };

typedef enum : uint8_t { HELD_COLOURS, HELD_SHOWN, HELD_CODES } IndicesHeld;

/* Index j's code, &HRRGGBB: its 565 word j times 0x0101. */
uint32_t indices_code(uint32_t j);
/* The index a code read back as R, G and B (accel/driver.h's read) is. */
uint8_t indices_of_code(const uint8_t *rgba);
/* Palette entry i, &HRRGGBB, an INDEX texel draws through into a target holding held: lookup's
   word, or i itself when null, through colours when shown. */
static inline uint32_t indices_entry(
    const uint32_t i,
    const uint32_t *const lookup,
    const uint32_t colours[INDICES],
    const IndicesHeld held
)
{
    const uint32_t w = lookup ? lookup[i] : i;

    return held == HELD_COLOURS ? w & INDICES_RGB
           : held == HELD_SHOWN ? colours[(uint8_t)w] & INDICES_RGB
                                : indices_code(w);
}
/* A vertex's colour argb into a target holding held: an index target's index, red, as what it
   holds; its alpha kept. */
uint32_t indices_colour(uint32_t argb, const uint32_t colours[INDICES], IndicesHeld held);
/* The index of colours nearest a pixel read back as R, G and B, memo's cell for its 565 word
   kept, an index and 1, 0 not yet. */
uint8_t indices_nearest(const uint8_t *rgba, const uint32_t colours[INDICES], uint16_t *memo);
#endif
