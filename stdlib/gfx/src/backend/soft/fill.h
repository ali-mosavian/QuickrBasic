/* Fillers: spans and rows to pixels, in dispatch tables as uGL's CFMT. A colour format has a
 * table; in it a kind of row has a filler for each variant of the flags, fused, so nothing below
 * the pick tests a flag. The scanner and the backend know no format: a new one is a new table.
 *
 * # How it works
 * fill_formats[format] is the format's table, a row's filler picked once a call by its variant:
 * FILL_FLIPX reads the texels backward from the first, FILL_OPAQUE draws index 0 where keyed rows
 * skip it. A turned or scaled picture is a walk (steps.h), its flips already in its steps. A call
 * with an ink to go through (ink.h) has an inked filler, uGL's hlinet: by the source's format and
 * the ink's variant (fill_variant), fetch and ink fused, the ink's tables in registers and no flag
 * tested a texel. ink is the same a texel at a time, for the spans drawn so.
 */
#ifndef GFX_FILL_H
#define GFX_FILL_H
#include "base/pixels.h"
#include "backend/soft/ink.h"
#include "backend/soft/steps.h"

enum { FILL_OPAQUE = 1, FILL_FLIPX = 2, FILL_VARIANTS = 4 };

enum { FILL_CHUNK = 256 }; /* pixels the ink takes at once */

/* An inked filler's variant: texel 0 skipped, every texel the ink's colour, an 8-bit texel through
   the lut, blended over the pixel there. */
enum { FILL_KEYED = 1, FILL_SOLID = 2, FILL_LUT = 4, FILL_BLENDED = 8, FILL_INKS = 16 };

/* n pixels of a colour of this format. */
typedef void FillSpan(uint8_t *d, int32_t n, uint32_t colour);
/* n texels from s, its first read; FLIPX variants step back from it. */
typedef void FillRow(uint8_t *d, const uint8_t *s, int32_t n);
/* n texels of s along the walk, inside s. */
typedef void FillStep(uint8_t *d, const Pixels *s, Steps st, int32_t n);
/* n texels through the ink into d, of this format: the steps of uGL's tfx row pipeline. */
typedef void FillInk(uint8_t *d, const uint32_t *texels, int32_t n, const Ink *k);
/* One texel through the ink but its blend: this format's colour. */
typedef uint32_t FillPaint(uint32_t texel, const Ink *k);
/* n texels of s along the walk, inside s, through the ink into d: fetch and ink as one. */
typedef void FillInked(uint8_t *d, const Pixels *s, Steps st, int32_t n, const Ink *k);
/* n pixels of a shape's colour through the ink, blended over those there. */
typedef void FillTint(uint8_t *d, int32_t n, uint32_t colour, const Ink *k);

/* A format's, drawn into. The fused row and step fillers read texels of this format; step's
   variants are FILL_OPAQUE's. inked is by the source's format and the variant. */
typedef struct {
    FillSpan *span;
    FillRow *row[FILL_VARIANTS];
    FillStep *step[FILL_OPAQUE + 1];
    FillInk *ink;
    FillPaint *paint;
    FillInked *const (*inked)[FILL_INKS];
    FillTint *tint;
} Fill;

/* k's variant for texels of format from into a target of format into. */
static inline uint32_t fill_variant(const Ink *const k, const PxFormat into, const PxFormat from)
{
    const bool lut = into == PX_8 && from == PX_8 && k->lut && !k->solid;

    return (k->keyed ? FILL_KEYED : 0) | (k->solid ? FILL_SOLID : 0) | (lut ? FILL_LUT : 0) |
           (k->blend || k->mode ? FILL_BLENDED : 0);
}

/* By format. */
extern const Fill *const fill_formats[PX_FORMATS];
extern const Fill fill8, fill15, fill16, fill32;

/* The ARGB of n texels through the ink but its blend, and which are drawn: a direct target's
   first steps. */
void fill_argb(uint32_t *argb, bool *drawn, const uint32_t *texels, int32_t n, const Ink *k);
/* s drawn over d, both ARGB, by the ink's mode and alpha. */
uint32_t fill_blend(uint32_t s, uint32_t d, const Ink *k);
#endif
