/* The software backend: each command drawn as it comes, into memory.
 *
 * # How it works
 * Shapes go through the scanner (scan.h) to the target's fillers (fill.h). An upright sprite at
 * 1:1 skips the scanner: its rows are copied, keyed unless OPAQUE, read backward for FLIPX. Points
 * are pixel centres, as the scanner's.
 */
#ifndef GFX_SOFT_H
#define GFX_SOFT_H
#include "pixels.h"
#include "soft/fill.h"

/* What is drawn into: its pixels, the clip in them, its depth's fillers. */
typedef struct {
    Pixels px;
    Rect clip;
    const Fill *fill;
} SoftTarget;

void soft_clear(const SoftTarget *t, uint32_t colour);
void soft_pset(const SoftTarget *t, float x, float y, uint32_t colour);
/* Both ends drawn. */
void soft_line(const SoftTarget *t, Vec a, Vec b, uint32_t colour);
/* A closed outline through n points, each pixel once. */
void soft_outline(const SoftTarget *t, const Vec *p, uint32_t n, uint32_t colour);
/* A convex polygon of n points, filled. */
void soft_poly(const SoftTarget *t, const Vec *p, uint32_t n, uint32_t colour);
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
/* s with its top left texel at x, y; flags draw.FLIPX, draw.FLIPY, pen.OPAQUE. */
void soft_sprite(const SoftTarget *t, const Pixels *s, float x, float y, int32_t flags);
/* An 8 by h glyph, a byte a row, the leftmost pixel the high bit, its top left at x, y. */
void soft_glyph(
    const SoftTarget *t,
    const uint8_t *rows,
    int32_t h,
    float x,
    float y,
    uint32_t colour
);
#endif
