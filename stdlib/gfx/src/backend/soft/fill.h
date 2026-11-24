/* Fillers: spans and rows to pixels, in dispatch tables as uGL's CFMT. A colour format has a
 * table; in it a kind of row has a filler for each variant of the flags, fused, so nothing below
 * the pick tests a flag. The scanner and the backend know no format: a new one is a new table.
 *
 * # How it works
 * fill_formats[format] is the format's table, a row's filler picked once a call by its variant:
 * FILL_FLIPX reads the texels backward from the first, FILL_OPAQUE draws index 0 where keyed rows
 * skip it. A turned or scaled picture is a walk (steps.h), its flips already in its steps. A call
 * with an ink to go through (ink.h) has its texels fetched, then coloured, keyed, blended and
 * stored, FILL_CHUNK at a time.
 */
#ifndef GFX_FILL_H
#define GFX_FILL_H
#include "base/pixels.h"
#include "backend/soft/ink.h"
#include "backend/soft/steps.h"

enum { FILL_OPAQUE = 1, FILL_FLIPX = 2, FILL_VARIANTS = 4 };

enum { FILL_CHUNK = 256 }; /* pixels the ink takes at once */

/* n pixels of a colour of this format. */
typedef void FillSpan(uint8_t *d, int32_t n, uint32_t colour);
/* n texels from s, its first read; FLIPX variants step back from it. */
typedef void FillRow(uint8_t *d, const uint8_t *s, int32_t n);
/* n texels of s along the walk, inside s. */
typedef void FillStep(uint8_t *d, const Pixels *s, Steps st, int32_t n);
/* n texels of s, of this format, along the walk, inside s: their values. */
typedef void FillFetch(uint32_t *out, const Pixels *s, Steps st, int32_t n);
/* n texels through the ink into d, of this format: the steps of uGL's tfx row pipeline. */
typedef void FillInk(uint8_t *d, const uint32_t *texels, int32_t n, const Ink *k);
/* One texel through the ink but its blend: this format's colour. */
typedef uint32_t FillPaint(uint32_t texel, const Ink *k);

/* A format's: drawn into (span, row, step, ink, paint) and drawn from (fetch). The fused row and
   step fillers read texels of this format; step's variants are FILL_OPAQUE's. */
typedef struct {
    FillSpan *span;
    FillRow *row[FILL_VARIANTS];
    FillStep *step[FILL_OPAQUE + 1];
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
