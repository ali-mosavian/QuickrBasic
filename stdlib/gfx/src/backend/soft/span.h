/* Spans: a shape's rows into a call's target in one colour, by a flat row filler (fill.h) picked
   once a call. */
#ifndef GFX_SPAN_H
#define GFX_SPAN_H
#include "backend/soft/scanner.h"
#include "backend/soft/soft.h"

uint8_t *span_at(const SoftTarget *t, int32_t x, int32_t y);
/* The ink has nothing to do with s's texels: the fused fillers draw them. */
bool span_fused(const SoftTarget *t, const Pixels *s);
/* A flat call's filler and its rows, picked once a call: the colour through the ink. */
typedef struct {
    Rows r;
    RowFlat *fill;
} Flat;

Flat span_flat_of(const SoftTarget *t, uint32_t colour);
/* Row y from x0 up to x1 by f, clipped. */
void span_flat(const SoftTarget *t, const Flat *f, int32_t y, int32_t x0, int32_t x1);
#endif
