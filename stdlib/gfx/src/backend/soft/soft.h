/* The software renderer: each command drawn as it comes, into memory.
 *
 * # How it works
 * Shapes are flat spans (shapes.c); polygons go through the scanner (scanner.h) to spans (span.h),
 * flat or along a picture's texels (poly.c). An upright sprite at 1:1 skips the scanner: its rows
 * are copied, keyed unless OPAQUE, read backward for FLIPX (sprite.c). Points are pixel centres,
 * as the scanner's.
 */
#ifndef GFX_SOFT_H
#define GFX_SOFT_H
#include "base/depth.h"
#include "base/pixels.h"
#include "backend/soft/fill.h"
#include "backend/soft/scanner.h"

/* What is drawn into: its pixels, the clip in them, its depth's fillers; the call's ink and the
   tint table of its blended shapes. */
typedef struct {
    Pixels px;
    Rect clip;
    const Fill *fill;
    Ink ink;
    FillTints *tints;
    Depth depth; /* p null: none asked */
} SoftTarget;

void soft_clear(const SoftTarget *t, uint32_t colour);
void soft_pset(const SoftTarget *t, float x, float y, uint32_t colour);
/* Both ends drawn. */
void soft_line(const SoftTarget *t, Vec a, Vec b, uint32_t colour);
/* A closed outline through n points, each pixel once. */
void soft_outline(const SoftTarget *t, const Vec *p, uint32_t n, uint32_t colour);
/* A convex polygon of n corners, 3 up to SCAN_MAXV: s's texels, affine or with draw.PERSP in
   perspective, keyed unless OPAQUE, u repeating when wrap, else u and v inside s; s null, colour.
   ZTEST draws a pixel only nearer than t's depth there, ZSET writes the depth of each drawn. */
void soft_poly(
    const SoftTarget *t,
    const Pixels *s,
    const Corner *c,
    uint32_t n,
    uint32_t colour,
    int32_t flags,
    bool wrap
);
/* The circle about c of radius r: filled, the pixels whose centres lie within r and a half; else
   its outline, those from r less a half. */
void soft_circle(const SoftTarget *t, Vec c, float r, uint32_t colour, bool fill);
/* The outline's pixels from the angle start clockwise on the screen to finish, radians; a whole
   turn or more, all of it. */
void soft_arc(const SoftTarget *t, Vec c, float r, float start, float finish, uint32_t colour);
/* Corners x1, y1 and x2, y2 drawn, in any order; filled or outlined. */
void soft_rect(
    const SoftTarget *t,
    float x1,
    float y1,
    float x2,
    float y2,
    uint32_t colour,
    bool fill
);
/* s with its top left texel at x, y; flags draw.FLIP_X, draw.FLIP_Y, draw.OPAQUE. */
void soft_sprite(const SoftTarget *t, const Pixels *s, float x, float y, int32_t flags);
#endif
