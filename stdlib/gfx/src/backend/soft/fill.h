/* Fillers: spans and rows to pixels, in dispatch tables as uGL's CFMT. A colour format has a
 * table; in it a kind of row has a filler for each variant of the flags, fused, so nothing below
 * the pick tests a flag. The scanner and the backend know no format: a new one is a new table.
 *
 * # How it works
 * fill_formats[format] is the format's table, a row's filler picked once a call by its variant:
 * FILL_FLIPX reads the texels backward from the first, FILL_OPAQUE draws index 0 where keyed rows
 * skip it. A polygon's row is a walk (steps.h), its flips already in its steps, filled by one of
 * the row fillers (RowFlat, RowAffine, RowPersp) its walker calls a row. A call with an ink to go
 * through (ink.h) has an inked filler, uGL's hlinet: by the source's format and the ink's variant
 * (fill_variant), fetch and ink fused, the ink's tables in registers and no flag tested a texel.
 * ink is the same a texel at a time, SMOOTH's.
 */
#ifndef GFX_FILL_H
#define GFX_FILL_H
#include "base/depth.h"
#include "base/pixels.h"
#include "backend/soft/ink.h"
#include "backend/soft/steps.h"

enum { FILL_OPAQUE = 1, FILL_FLIPX = 2, FILL_VARIANTS = 4 };

/* An inked filler's variant: texel 0 skipped, every texel the ink's colour, an 8-bit texel through
   the lut, blended over the pixel there. */
enum { FILL_KEYED = 1, FILL_SOLID = 2, FILL_LUT = 4, FILL_BLENDED = 8, FILL_INKS = 16 };

/* n texels from s, its first read; FLIPX variants step back from it. */
typedef void FillRow(uint8_t *d, const uint8_t *s, int32_t n);
/* n texels through the ink into d, of this format: the steps of uGL's tfx row pipeline. */
typedef void FillInk(uint8_t *d, const uint32_t *texels, int32_t n, const Ink *k);
/* A blended colour's pixel over each pixel there, by its channels as px_table's: made for its
   colour, mode, alpha and format, again only when one changes, so once a shape. */
typedef struct {
    uint32_t pixels[PX_TABLE];
    uint32_t colour;
    BlendMode mode;
    uint8_t alpha;
    PxFormat format;
    bool made;
} FillTints;

/* A blended direct target's mix by table: each channel a texel's share by px_table's entries,
   then the pixel there's by the target's, their sum the channel; under ADD, the sums' entries
   after, each held at 255 and a channel. An 8-bit texel's share is its colour's, as 32 bits'.
   Made for the source, shade, target, mode and alpha, again only when one changes, so once a
   shape. */
enum { FILL_SUMS = 512, FILL_MIXES = 2 * PX_TABLE + 3 * FILL_SUMS };

typedef struct {
    uint32_t table[FILL_MIXES];
    PxFormat from, into;
    uint32_t shade;
    BlendMode mode;
    uint8_t alpha;
    bool made;
} FillMixes;

/* k's mix table into a target of direct format into, made in m unless made already. */
const uint32_t *fill_mixes(FillMixes *m, PxFormat into, const Ink *k);

/* SMOOTH's colours of a direct texture's texels, lit, by px_table's channels: made for the
   source and shade, again only when one changes. */
typedef struct {
    uint32_t table[PX_TABLE];
    PxFormat from;
    uint32_t shade;
    bool made;
} FillColours;

/* k's colours of its direct source, made in c unless made already. */
const uint32_t *fill_colours(FillColours *c, const Ink *k);

/* A call's, read by its row fillers: set once a call, as uGL patches its fillers. */
typedef struct {
    Pixels dst, src;
    Depth depth;
    const Ink *ink;
    uint32_t depths;        /* the general fillers': ROW_'s, 0 when no depth */
    uint32_t colour;        /* flat: the pixel, or blended the colour through the ink */
    const uint8_t *blend;   /* flat, blended into 8 bits: the colour's row of the blend table */
    const uint32_t *tints;  /* flat, blended into a direct target: FillTints' pixels */
    int32_t wrap;           /* u repeats every wrap (16.16), 0 never */
    int32_t wrap_v;         /* v repeats every wrap_v (16.16), 0 never */
    int32_t du, dv, dz;     /* affine: u and v 16.16 a pixel; depth's a pixel */
    float dpun, dpvn, dpzn; /* perspective: u/z, v/z and 1/z a run's, SCAN_SUBDIVP pixels */
} Rows;

/* A row filler's depth: tested, written, both or neither. */
enum { ROW_TEST = 1, ROW_WRITE = 2, ROW_DEPTHS = 4 };
/* An affine row's walk: inside the texture, held inside it, or wrapping across it. */
enum { ROW_PLAIN, ROW_HELD, ROW_WRAPPED, ROW_WALKS };

/* Row y's pixels x0 up to x1 of a polygon in r, its texels from u, v (16.16) and its depth from z
   (16.16) at x0: one loop. */
typedef void RowAffine(
    const Rows *r,
    int32_t y,
    int32_t x0,
    int32_t x1,
    int32_t u,
    int32_t v,
    int32_t z
);
/* Row y's pixels x0 up to x1 of a flat shape in r, its depth from z (16.16) at x0: one loop. */
typedef void RowFlat(const Rows *r, int32_t y, int32_t x0, int32_t x1, int32_t z);
/* The flat row filler for r's ink and depths, the colour through the ink once into r; blended
   into a direct target, by tints, made first unless made already. */
typedef RowFlat *FillFlat(Rows *r, uint32_t colour, FillTints *tints);
/* As RowAffine, in perspective: u/z, v/z and 1/z at x0, a divide every SCAN_SUBDIVP pixels. */
typedef void RowPersp(
    const Rows *r,
    int32_t y,
    int32_t x0,
    int32_t x1,
    double u,
    double v,
    double w,
    int32_t z
);

/* A format's, drawn into. flat, a colour by depth (ROW_); pick_flat picks it, or a blended
   one. The fused row fillers read texels of this format: affine and persp, by FILL_OPAQUE, depth
   and, affine's, walk (ROW_PLAIN, HELD or WRAPPED). inked,
   by the source's format and the variant, draws an affine row inside its texture, no depth.
   general draws any other through the ink a pixel at a time, by Rows' depths and wrap; smooth,
   SMOOTH's. */
typedef struct {
    FillFlat *pick_flat;
    RowFlat *flat[ROW_DEPTHS];
    FillRow *row[FILL_VARIANTS];
    FillInk *ink;
    RowAffine *const (*inked)[FILL_INKS];
    RowAffine *affine[FILL_OPAQUE + 1][ROW_DEPTHS][ROW_WALKS];
    RowPersp *persp[FILL_OPAQUE + 1][ROW_DEPTHS];
    RowAffine *general, *smooth;
    RowPersp *general_persp, *smooth_persp;
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

/* The ARGB of n texels through the ink but its blend, by its argb, and which are drawn: SMOOTH's
   first steps. */
void fill_argb(uint32_t *argb, bool *drawn, const uint32_t *texels, int32_t n, const Ink *k);
#endif
