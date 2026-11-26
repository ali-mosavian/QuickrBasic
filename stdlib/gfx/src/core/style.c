/* The style (style.h). */
#include "core/style.h"

enum { FULL = 63, WHITE = 0xFFFFFF };

void style_reset(Style *const s) { *s = (Style){.light = FULL, .alpha = FULL, .tint = WHITE}; }
