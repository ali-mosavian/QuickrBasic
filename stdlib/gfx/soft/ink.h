/* Inks: what a draw call's pixels go through, made once for the call from its flags and the pen.
 *
 * # How it works
 * An ink's texels are its source's (an index, or a pixel of 15, 16 or 32 bits); a shape's colour
 * is one texel, an index at 8 bits, else 32 bits' &HRRGGBB.
 * - effects: something to do but change format; none, and source the target's format, the fused
 *   fillers draw it.
 * - keyed: a texel 0, or 32 bits' alpha under a half, is not drawn.
 * - solid: every texel drawn is colour, the target's index or &HFFRRGGBB.
 * - An 8-bit target: lut, the index each index draws as; map, 5 bits a channel to the nearest
 *   index; blend, 256 rows by the index drawn and 256 by the one there.
 * - Another: argb, the colour each index draws as.
 * - shade: light and tint multiplied into a direct texel (&HFFFFFF none); mode and alpha, the
 *   blend of a direct target.
 */
#ifndef GFX_INK_H
#define GFX_INK_H
#include "base/pixels.h"
#include "core/palette.h"
#include "core/pen.h"

typedef struct {
    PxFormat source;
    bool effects, keyed, solid;
    uint32_t colour;
    const uint8_t *lut, *map, *blend;
    const uint32_t *argb;
    uint32_t shade;
    BlendMode mode;
    uint8_t alpha;
} Ink;

/* The ink of a call's flags and the pen p into a target of format, its texels a shape's colours:
   false, no memory. */
bool ink_begin(Ink *k, Palette *pal, const Pen *p, int32_t flags, PxFormat format);
/* k readied for a picture's texels, of format from, into a target of format into: false, no
   memory. */
bool ink_source(Ink *k, Palette *pal, const Pen *p, PxFormat into, PxFormat from);
/* Every texel drawn is colour, a shape's, through the pen: SOLID's. */
void ink_solid(Ink *k, const Pen *p, PxFormat into, uint32_t colour);
#endif
