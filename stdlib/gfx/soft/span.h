/* Spans: a row's run of pixels into a call's target, in one colour or along a picture's texels,
 * the one way the software renderer's parts draw.
 *
 * # How it works
 * A textured span is a walk (steps.h): drawn by the target's fused filler when the ink has nothing
 * to do, else FILL_CHUNK texels at a time through it, when the walk stays inside the picture and
 * there is no depth; else a pixel at a time: its texel held inside, keyed, tested against the
 * depth there, drawn, its depth set. A perspective span is runs of SCAN_SUBDIVP pixels, each a
 * walk between two divides (qgl's PTEX): a divide's u = (u/z) / (1/z) rounds (fistp) and takes
 * half a texel on its own side. A run's ends are not clamped, as the last run's end lies a whole
 * run on, past the span, and clamped would bend its step.
 */
#ifndef GFX_SPAN_H
#define GFX_SPAN_H
#include "soft/scanner.h"
#include "soft/soft.h"

/* A textured span's: the call's target, the picture, and the call's flags. */
typedef struct {
    const SoftTarget *t;
    const Pixels *s;
    bool opaque, wrap, test, set;
} Textured;

uint8_t *span_at(const SoftTarget *t, int32_t x, int32_t y);
/* The ink has nothing to do with s's texels: the fused fillers draw them. */
bool span_fused(const SoftTarget *t, const Pixels *s);
/* Row y from x0 up to x1 in colour, clipped. */
void span_flat(const SoftTarget *t, int32_t y, int32_t x0, int32_t x1, uint32_t colour);
/* n pixels from x, y, inside the clip, along st's texels of x's picture, u wrapped already when
   it wraps; their depth from z by dz a pixel, tested and set as x says. */
void span_texels(
    const Textured *x,
    int32_t px,
    int32_t y,
    int32_t n,
    Steps st,
    int32_t z,
    int32_t dz
);
/* A perspective span (scanner.h), inside the clip, in runs. */
void span_persp(const Textured *x, const ScanSpan *sp);
#endif
