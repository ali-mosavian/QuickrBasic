/* A page of the graphics screen in memory, a byte a pixel: gfxdev.h's drawing over it (pixmem.c). The host's
   screen is all such pages; DOS's a hidden page of a mode the adapter holds one page of. GET and PUT keep the
   adapters' layouts. */
#ifndef QB_PIXMEM_H
#define QB_PIXMEM_H

#include "gfxdev.h"

enum { PM_PLANAR, PM_LINEAR, PM_PACKED4, PM_PACKED2 };

/* The page drawn on: its pixels, size and kind (PM_*); touch(top, count) is told of each run of rows drawn. */
void pm_use(u8 *pixels, unsigned width, unsigned height, unsigned kind, void (*touch)(unsigned top, unsigned count));

void pm_plot(unsigned x, unsigned y, unsigned color, unsigned operation);
unsigned pm_read(unsigned x, unsigned y);
int pm_search(int x, int last, unsigned y, unsigned c1, unsigned c2, int match);
int pm_run(int x, int last, unsigned y, unsigned edge, unsigned paint, int limit, int *left, int *right);
void pm_move_rows(unsigned to, unsigned from, unsigned count);
void pm_glyph(unsigned x, unsigned y, const u8 *bits, unsigned rows, unsigned foreground);
void pm_fill_select(GdFill *fill, unsigned color, unsigned operation);
void pm_get(unsigned x, unsigned y, unsigned box_width, unsigned rows, u8 *out);
void pm_put(unsigned x, unsigned y, unsigned box_width, unsigned rows, const u8 *in, unsigned operation,
            unsigned invert);

#endif
