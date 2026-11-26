/* The style: draw's settings, the numbers a call's colours go through, for every call after. */
#ifndef GFX_STYLE_H
#define GFX_STYLE_H
#include <stdint.h>

typedef struct {
    int32_t light, alpha, palette_row; /* 63, 63, 0: a texel's own */
    uint32_t tint, font;               /* &HFFFFFF; 0 the built-in */
} Style;

void style_reset(Style *s);
#endif
