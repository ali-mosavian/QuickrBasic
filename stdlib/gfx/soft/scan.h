/* Pixel rules for points and shapes the scanner (rs.h) does not draw.
 *
 * # How it works
 * A coordinate names a pixel's centre: a point is drawn at the pixel whose square holds it, and a
 * shape's edge at v takes the first pixel whose centre is at or past v. A polygon's spans are
 * qgl's instead: an edge's x and y round half up to a pixel, a span runs from the left edge's
 * pixel up to, not including, the right edge's, and rows likewise (rs.h).
 */
#ifndef GFX_SCAN_H
#define GFX_SCAN_H
#include "pixels.h"

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
#endif
