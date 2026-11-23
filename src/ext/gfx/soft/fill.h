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
#include "pixels.h"

typedef enum : uint8_t { FILL_8, FILL_FORMATS } FillFormat;

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
   nothing. keyed: a texel 0 is not drawn. solid: every texel drawn is colour. */
typedef struct {
    const uint8_t *lut;   /* the index each texel draws as */
    const uint8_t *blend; /* 256 rows by the index drawn, 256 by the one there */
    bool keyed, solid;
    uint32_t colour;
} Ink;

/* Nothing to go through: the fused fillers draw it. */
static inline bool fill_plain(const Ink *const k) { return !k->lut && !k->blend && !k->solid; }

/* n pixels of colour. */
typedef void FillSpan(uint8_t *d, int32_t n, uint32_t colour);
/* n texels from s, its first read; FLIPX variants step back from it. */
typedef void FillRow(uint8_t *d, const uint8_t *s, int32_t n);
/* n texels of s along the walk. */
typedef void FillStep(uint8_t *d, const Pixels *s, Steps st, int32_t n);
/* n texels of s, of this format, along the walk: their values. */
typedef void FillFetch(uint32_t *out, const Pixels *s, Steps st, int32_t n);
/* n texels through the ink into d, of this format: the steps of uGL's tfx row pipeline. */
typedef void FillInk(uint8_t *d, const uint32_t *texels, int32_t n, const Ink *k);

typedef struct {
    FillSpan *span;
    FillRow *row[ROW_VARIANTS];
    FillStep *step[STEP_VARIANTS];
    FillFetch *fetch;
    FillInk *ink;
} Fill;

/* By format: null, none yet. */
extern const Fill *const fill_formats[FILL_FORMATS];
extern const Fill fill8;
#endif
