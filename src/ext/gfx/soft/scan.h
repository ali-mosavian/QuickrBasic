/* The scanner: a convex polygon to spans, clipped. It knows no depth; a filler draws the spans.
 *
 * # How it works
 * A coordinate names a pixel's centre. A row's span runs between its edges at the row's centre,
 * and a pixel is drawn when its centre is inside, or on the top or left edge: rows and columns are
 * half open, [first, last). A row's edges are those crossing it, the polygon being convex.
 */
#ifndef GFX_SCAN_H
#define GFX_SCAN_H
#include "pixels.h"

typedef struct {
    float x, y;
} ScanPoint;

/* Row y's pixels x0 up to x1, inside the clip. */
typedef void ScanSpan(void *ctx, int32_t y, int32_t x0, int32_t x1);

/* The first pixel whose centre is at or past v: a pixel on a top or left edge is drawn. */
static inline int32_t scan_first(const float v)
{
    const float most = 1 << 30;
    const float c = v < -most ? -most : v > most ? most : v;
    const int32_t i = (int32_t)c;
    return (float)i < c ? i + 1 : i;
}

/* The pixel whose square holds x. */
static inline int32_t scan_pixel(const float x) { return scan_first(x - 0.5f); }

void scan_convex(const ScanPoint *p, uint32_t n, Rect clip, ScanSpan *span, void *ctx);
#endif
