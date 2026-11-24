/* The pen: where drawing goes, and the numbers its flags use, for what is drawn after. */
#ifndef GFX_PEN_H
#define GFX_PEN_H
#include "base/pixels.h"

typedef struct {
    uint32_t target;
    Rect clip; /* in the target; clipped false, the whole of it */
    bool clipped;
    int32_t light, alpha, recolour; /* 63, 63, 0: a texel's own */
    uint32_t tint, font;            /* &HFFFFFF; 0 the built-in */
} Pen;

void pen_reset(Pen *p, uint32_t canvas);
/* The clip in a target w by h. */
Rect pen_clip(const Pen *p, int32_t w, int32_t h);
#endif
