/* qgl's convex scanner (qgl's rs.asm, qglRsPoly), translated: a polygon to one span a row.
 *
 * # How it works
 * The front half is float: the gradient triple, the winding and degeneracy cull (a polygon under
 * one pixel of area is dropped), a clip to the rectangle, then 16.16 vertices. The scanners are
 * integer: two edge chains walk down from the topmost vertex, one span a row.
 *
 * qgl's pixel rule: an edge's x and y round half up to a pixel (affine; the perspective scanner
 * adds no half, so it truncates), and a row's span runs from its left edge's pixel up to, not
 * including, its right edge's; rows likewise. An edge a row or more high has its slope divided,
 * floored, where qgl's multiplied by a truncated reciprocal and drifted; under a row, qgl's, a
 * sub-scanline edge's with 14 bits more.
 *
 * u and v are in texels (qgl scales them by the texture's size: here the scale is 1). Affine
 * spans carry u and v 16.16 at x0 and their step a pixel; u, v and 1/z are taken at the pixel
 * centre as qgl$drawA does. Perspective spans carry u/z, v/z and 1/z at x0 and their steps, as
 * qgl$drawP does, for persp.h.
 *
 * Left out: depth, EMS and slots, profiling, lightmaps, the cover and bound helpers, WIRE. The
 * x87's extended arithmetic is done in doubles, and its stores to real4 as floats.
 */
#ifndef GFX_SCANNER_H
#define GFX_SCANNER_H
#include "base/depth.h"
#include "base/pixels.h"

/* qgl's QVert: z is 1/z; 2D calls pass 1. */
typedef struct {
    float x, y, z, u, v;
} ScanVert;

typedef enum {
    SCAN_FLAT = 1, /* no u, v */
    SCAN_TEX,      /* affine */
    SCAN_PTEX,     /* perspective, a divide every SCAN_SUBDIVP pixels */
    SCAN_ATEX,     /* projective edges, affine a row */
} ScanMode;

enum { SCAN_MAXV = 41, SCAN_SUBDIVS = 4, SCAN_SUBDIVP = 16 };

/* A row's span, pixels x0 up to x1, not clipped to x: the caller holds it in. */
typedef struct {
    int32_t y, x0, x1;
    int32_t u, v, du, dv;   /* SCAN_TEX: 16.16 at x0, and a pixel's step */
    double pu, pv, pz;      /* SCAN_PTEX, SCAN_ATEX: u/z, v/z, 1/z at x0 */
    float dpu, dpv, dpz;    /* ... a pixel's step */
    float dpun, dpvn, dpzn; /* ... SCAN_SUBDIVP pixels' */
    int32_t z, dz;          /* depth, 16.16: at x0 (qgl's zacc), and a pixel's step */
} ScanSpan;

typedef void ScanFill(void *ctx, const ScanSpan *span);

/* qgl's fistp: the nearest integer, ties to even; out of range, 80000000h. llrint is a fistp on
   an x87, whose 64-bit mantissa a double's magic-number round does not round. */
static inline int32_t scan_fistp(const double x)
{
    if (!(x > -2147483648.5 && x < 2147483647.5))
        return INT32_MIN;

    return (int32_t)__builtin_llrint(x);
}

/* x as qgl's fistp into a qword, its low dword: a span's depth, which runs to 65535 in 16.16. */
static inline int32_t scan_fistp_low(const double x)
{
    return (int32_t)(uint32_t)(uint64_t)__builtin_llrint(x);
}

/* n vertices (3 up to SCAN_MAXV, either winding) of mode, clipped to clip, a span to fill each
   row. gfx: gfx's pixel rule, a centre on a top or left edge inside as a GPU's, where qgl's is on
   the bottom or right; and in perspective the half pixel qgl$F2fxP leaves out, so it covers as
   affine does. The rows covered, 0 for none; -1: refused. */
int32_t scan_poly(
    const ScanVert *v,
    uint32_t n,
    ScanMode mode,
    Rect clip,
    bool gfx,
    ScanFill *fill,
    void *ctx
);
#endif
