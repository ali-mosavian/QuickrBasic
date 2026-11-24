/* Fillers: spans and rows to pixels, in dispatch tables as uGL's CFMT. A colour format has a
 * table; in it a kind of row has a filler for each variant of the flags, fused, so nothing below
 * the pick tests a flag. The scanner and the backend know no format: a new one is a new table.
 *
 * # How it works
 * fill_formats[format] is the format's table. The backend turns a draw call's flags into a
 * variant once (soft.c) and calls row[variant] for each row. A variant's bits are the filler's
 * own, not the API's flags: FLIPX reads the texels backward from the first, OPAQUE draws index 0
 * where keyed rows skip it. A turned or scaled picture is a walk (Steps), its flips already in its
 * steps. A call with light, tint, a palette row, SOLID or a blend goes through the ink instead:
 * its texels fetched, then coloured, keyed, blended and stored, FILL_CHUNK at a time.
 */
#ifndef GFX_FILL_H
#define GFX_FILL_H
#include "base/pixels.h"

enum { ROW_FLIPX = 1, ROW_OPAQUE = 2, ROW_VARIANTS = 4 };
enum { STEP_OPAQUE = 1, STEP_VARIANTS = 2 };

/* A walk over a surface's texels, u and v 16.16: the first texel's, and the step a pixel. wrap:
   u repeats every wrap (16.16), 0 never. */
typedef struct {
    int32_t u, v, du, dv, wrap;
} Steps;

/* u, wrapped into [0, w). */
static inline int32_t fill_wrap(int32_t u, const int32_t w)
{
    while (u >= w)
        u -= w;

    while (u < 0)
        u += w;

    return u;
}

enum { FILL_CHUNK = 256 }; /* pixels the ink takes at once */

/* What a draw call's pixels go through, picked once for the call (gfx.c); a null table does
 * nothing. Its texels are source's (an index, or a pixel of 15, 16 or 32 bits); a shape's colour
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

/* n pixels of a colour of this format. */
typedef void FillSpan(uint8_t *d, int32_t n, uint32_t colour);
/* n texels from s, its first read; FLIPX variants step back from it. */
typedef void FillRow(uint8_t *d, const uint8_t *s, int32_t n);
/* n texels of s along the walk. */
typedef void FillStep(uint8_t *d, const Pixels *s, Steps st, int32_t n);
/* n texels of s, of this format, along the walk: their values. */
typedef void FillFetch(uint32_t *out, const Pixels *s, Steps st, int32_t n);
/* n texels through the ink into d, of this format: the steps of uGL's tfx row pipeline. */
typedef void FillInk(uint8_t *d, const uint32_t *texels, int32_t n, const Ink *k);
/* One texel through the ink but its blend: this format's colour. */
typedef uint32_t FillPaint(uint32_t texel, const Ink *k);

/* A format's: drawn into (span, row, step, ink, paint) and drawn from (fetch). The fused row and
   step fillers read texels of this format. */
typedef struct {
    FillSpan *span;
    FillRow *row[ROW_VARIANTS];
    FillStep *step[STEP_VARIANTS];
    FillFetch *fetch;
    FillInk *ink;
    FillPaint *paint;
} Fill;

/* By format. */
extern const Fill *const fill_formats[PX_FORMATS];
extern const Fill fill8, fill15, fill16, fill32;

/* The ARGB of n texels through the ink but its blend, and which are drawn: a direct target's
   first steps. */
void fill_argb(uint32_t *argb, bool *drawn, const uint32_t *texels, int32_t n, const Ink *k);
/* s drawn over d, both ARGB, by the ink's mode and alpha. */
uint32_t fill_blend(uint32_t s, uint32_t d, const Ink *k);
#endif
