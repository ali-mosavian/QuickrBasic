/* gfx's screen, QxHost's gfx_* (io/native.c): the page an extension draws in, and the frame it
 * showed last, which a display reads (plat/view/frame.c).
 *
 * # How it works
 * The page is memory the host lends. pgfx_show copies it to the frame shown, so a display's thread
 * reads a whole frame while the next is drawn, and the page keeps its pixels. A terminal shows
 * nothing: unit tests read the page. A pixel is an index (8 bits, the frame's palette its colours),
 * x555 (15), 565 (16) or &HAARRGGBB (32), little-endian, its alpha unshown.
 *
 * A display with a GPU sets a player before the program runs (plat/sdl/player.c): a screen it opens
 * plays the extension's GPU lists (qx/qxlist.h) instead, and shows what they show.
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

/* A GPU's screen: the shading languages it takes (1 << QxShader each), and whether clip space's y
   runs down a texture's rows; opened w by h, closed; a list played; w by h pixels at x, y of a
   texture read, bytes R, G, B and A. Called from the interpreter's thread. */
typedef struct {
    uint8_t shaders;
    bool y_down;
    bool (*open)(uint16_t w, uint16_t h);
    void (*close)(void);
    void (*play)(const uint32_t *list, uint32_t words);
    void (*read)(uint32_t texture, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *out);
} PgfxPlayer;

void pgfx_set_player(const PgfxPlayer *p);
/* A screen w by h the player plays, in the runtime's place, when it takes one of shaders: the
   player; null, none, or it could not. Open, a screen is closed first. */
const PgfxPlayer *pgfx_open_played(uint16_t w, uint16_t h, uint8_t shaders);
/* The open screen's player: null, none or pages. */
const PgfxPlayer *pgfx_playing(void);
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
