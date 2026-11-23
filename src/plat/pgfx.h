/* gfx's screen, QxHost's gfx_* (io/native.c): the page an extension draws in, and the frame it
 * showed last, which a display reads (plat/view/frame.c).
 *
 * # How it works
 * The page is memory the host lends. pgfx_show copies it to the frame shown, so a display's thread
 * reads a whole frame while the next is drawn, and the page keeps its pixels. A terminal shows
 * nothing: unit tests read the page. A pixel is an index (8 bits, the frame's palette its colours),
 * x555 (15), 565 (16) or &HAARRGGBB (32), little-endian, its alpha unshown.
 */
#ifndef PLAT_PGFX_H
#define PLAT_PGFX_H
#include "base.h"

/* A frame shown: w by h pixels of depth bits, w a row; at 8 bits its 256 colours, &HRRGGBB. */
typedef struct {
    const uint8_t *pixels;
    uint16_t w, h;
    uint8_t depth;
    const uint32_t *palette;
} PgfxFrame;

/* A screen w by h, depth bits a pixel, in the runtime's place: false, none of that depth, or no
   memory. Open, it is closed first. */
bool pgfx_open(uint16_t w, uint16_t h, uint8_t depth);
void pgfx_close(void);
/* The page to draw in, *pitch bytes a row: null, no screen open. */
uint8_t *pgfx_page(uint32_t *pitch);
void pgfx_show(const uint32_t *palette);
/* The frame shown last, while the screen is open: false, none. */
bool pgfx_shown(PgfxFrame *f);
#endif
