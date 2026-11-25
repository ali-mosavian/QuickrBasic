/* qgl's convex scanner (qgl's rs.asm, qglRsPoly), translated: a polygon to one span a row.
 *
 * # How it works
 * The front half is float: the gradient triple, the winding and degeneracy cull (a polygon under
 * one pixel of area is dropped), a clip to the rectangle, then 16.16 vertices. The scanners are
 * integer: two edge chains walk down from the topmost vertex, one span a row.
 *
 * gfx's pixel rule, a GPU's: a pixel is covered when its centre is inside, a centre on a top or
 * left edge inside. qgl's is on the bottom or right, its perspective scanner a half pixel off: an
 * edge's x and y round half up to a pixel, less a unit, and a row's span runs from its left edge's
 * pixel up to, not including, its right edge's; rows likewise. An edge a row or more high has its
 * slope divided, floored, where qgl's multiplied by a truncated reciprocal and drifted; under a
 * row, qgl's, a sub-scanline edge's with 14 bits more.
 *
 * u and v are in texels (qgl scales them by the texture's size: here the scale is 1). Affine
 * spans carry u and v 16.16 at x0 and their step a pixel; u, v and 1/z are taken at the pixel
 * centre as qgl$drawA does. Perspective spans carry u/z, v/z and 1/z at x0 and their steps, as
 * qgl$drawP does, to a row filler (fill.h) picked once a polygon. A span is held in the clip,
 * its values a pixel on for each it lost.
 *
 * Left out: depth, EMS and slots, profiling, lightmaps, the cover and bound helpers, WIRE. The
 * x87's extended arithmetic is done in doubles, and its stores to real4 as floats.
 */
#ifndef GFX_SCANNER_H
#define GFX_SCANNER_H
#include "base/depth.h"
#include "base/pixels.h"
#include "backend/soft/fill.h"

/* qgl's QVert: z is 1/z; 2D calls pass 1. */
typedef struct {
    float x, y, z, u, v;
} ScanVert;

enum { SCAN_MAXV = 41, SCAN_SUBDIVS = 4, SCAN_SUBDIVP = 16 };

/* x to the nearest integer, ties to even. llrint is a fistp on an x87, whose 64-bit mantissa a
   double's magic-number round does not round; wasm has no llrint, but rint is f64.nearest. */
static inline int64_t scan_rint(const double x)
{
#ifdef __wasm__
    return (int64_t)__builtin_rint(x);
#else
    return __builtin_llrint(x);
#endif
}

/* qgl's fistp: the nearest integer, ties to even; out of range, 80000000h. On an x87, the fistp
   itself: out of range it stores 80000000h. */
static inline int32_t scan_fistp(const double x)
{
#ifdef __i386__
    int32_t r;
    __asm__("fistpl %0" : "=m"(r) : "t"(x) : "st");
    return r;
#else

    if (!(x > -2147483648.5 && x < 2147483647.5))
        return INT32_MIN;

    return (int32_t)scan_rint(x);
#endif
}

/* x as qgl's fistp into a qword, its low dword: a span's depth, which runs to 65535 in 16.16. */
static inline int32_t scan_fistp_low(const double x)
{
    return (int32_t)(uint32_t)(uint64_t)scan_rint(x);
}

/* A perspective divide's: 65536 u/z and 65536 v/z rounded, and half a texel on this side of the
   divide (qgl's PDIV). */
typedef struct {
    int32_t u, v;
} ScanUV;

static inline ScanUV scan_pdiv(const double u, const double v, const double z)
{
    const double zf = 65536.0 / z;

    return (ScanUV){
        (int32_t)((uint32_t)scan_fistp(zf * u) + 32768u),
        (int32_t)((uint32_t)scan_fistp(zf * v) + 32768u),
    };
}

/* A run's step a pixel, from one divide to the next, SCAN_SUBDIVP pixels on (qgl's PMKSTEP). */
static inline int32_t scan_step(const int32_t from, const int32_t to)
{
    return (int32_t)((uint32_t)to - (uint32_t)from) >> SCAN_SUBDIVS;
}

/* n vertices (3 up to SCAN_MAXV, either winding) of a flat polygon in r, clipped to clip, scanned
   top to bottom: each row's run filled by fill. r's depth step is set. */
void scan_flat(const ScanVert *v, uint32_t n, Rect clip, Rows *r, RowFlat *fill);
/* As scan_flat, a picture's, affine: each row's run filled by plain, or by held where its walk
   leaves the picture. r's steps are set. */
void scan_affine(
    const ScanVert *v,
    uint32_t n,
    Rect clip,
    Rows *r,
    RowAffine *plain,
    RowAffine *held
);
/* As scan_affine, in perspective: u and v by 1/z, each row's run filled by fill. */
void scan_persp(const ScanVert *v, uint32_t n, Rect clip, Rows *r, RowPersp *fill);
#endif
