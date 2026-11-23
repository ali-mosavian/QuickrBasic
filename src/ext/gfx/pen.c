/* The pen (pen.h). */
#include "pen.h"

enum { FULL = 63, WHITE = 0xFFFFFF };

void pen_reset(Pen *const p, const uint32_t canvas)
{
    *p = (Pen){
        .target = canvas,
        .light = FULL,
        .alpha = FULL,
        .tint = WHITE,
    };
}

Rect pen_clip(const Pen *const p, const int32_t w, const int32_t h)
{
    const Rect all = {0, 0, w, h};

    return p->clipped ? rect_meet(p->clip, all) : all;
}
