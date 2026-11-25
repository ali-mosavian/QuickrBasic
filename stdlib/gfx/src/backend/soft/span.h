/* Spans: a row's run of pixels into a call's target in one colour. A textured row is a filler's
   (fill.h), picked once a call. */
#ifndef GFX_SPAN_H
#define GFX_SPAN_H
#include "backend/soft/scanner.h"
#include "backend/soft/soft.h"

uint8_t *span_at(const SoftTarget *t, int32_t x, int32_t y);
/* The ink has nothing to do with s's texels: the fused fillers draw them. */
bool span_fused(const SoftTarget *t, const Pixels *s);
/* Row y from x0 up to x1 in colour, clipped. */
void span_flat(const SoftTarget *t, int32_t y, int32_t x0, int32_t x1, uint32_t colour);
/* Row y from x0 up to x1 in colour, inside the clip; none when x1 is x0. */
void span_fill(const SoftTarget *t, int32_t y, int32_t x0, int32_t x1, uint32_t colour);
#endif
