/* Which pixels a circle, its outline and an arc cover: one rule for software (soft/shapes.c) and for
 * a GPU (gpu/vertex.h's DISC stage, gpu/shaders/draw.frag).
 *
 * # How it works
 * A pixel is within r of c when its centre p is: |p - c|^2 < r^2, or equal with p left of c (a
 * pixel on a left edge is drawn). A filled circle covers those within r + 1/2; an outline those
 * within that and not within r - 1/2; an arc the outline's within its sweep. Each product and sum
 * is rounded once, never fused, as the shader's are: the two agree to the pixel.
 */
#ifndef GFX_DISC_H
#define GFX_DISC_H
#include "base/gmath.h"
#include "base/geom.h"

/* The pixels within outer and not within inner of c. */
typedef struct {
    Vec c;
    float outer, inner;
} Disc;

/* An arc's sweep: from direction s clockwise to f; wide, past half a turn; whole, a turn or more. */
typedef struct {
    Vec s, f;
    bool wide, whole;
} Sweep;

/* The circle about c of radius r: filled, or its outline. */
static inline Disc disc_of(const Vec c, const float r, const bool fill)
{
    return (Disc){c, r + 0.5f, fill ? 0 : r - 0.5f};
}

/* d, from the centre, is within r. */
static inline bool disc_within(const float dx, const float dy, const float r)
{
#pragma clang fp contract(off)
    const float d = dx * dx + dy * dy, e = r * r;
    return r > 0 && (d < e || (d == e && dx < 0));
}

static inline bool disc_covers(const Disc *const k, const float dx, const float dy)
{
    return disc_within(dx, dy, k->outer) && !disc_within(dx, dy, k->inner);
}

/* From angle start clockwise to finish, in radians. */
static inline Sweep sweep_of(const float start, const float finish)
{
    const float turn = 2 * GM_PI, sweep = finish - start;
    Sweep w = {
        .wide = sweep - (float)gm_floor(sweep / turn) * turn > GM_PI,
        .whole = sweep >= turn,
    };

    gm_sincos(start, &w.s.y, &w.s.x);
    gm_sincos(finish, &w.f.y, &w.f.x);
    return w;
}

/* d, from the centre, lies in the sweep: within half a turn, clockwise of s and anticlockwise of
   f; past it, not strictly in the rest. */
static inline bool swept(const Sweep *const w, const float dx, const float dy)
{
#pragma clang fp contract(off)
    const float from = w->s.x * dy - w->s.y * dx, to = dx * w->f.y - dy * w->f.x;
    return w->whole || (w->wide ? !(from < 0 && to < 0) : from >= 0 && to >= 0);
}
#endif
