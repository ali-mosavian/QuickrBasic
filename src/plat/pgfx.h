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
 *
 * A screen opened with vsync, as it shows a frame, waits for the display to show one after: at
 * most a tenth of a second, as a hidden window shows none.
 *
 * The editor hides an open screen at a stop and shows it again when the run goes on.
 *
 * A display also sets its monitors and their modes; a screen it shows full screen is in a window
 * again when it closes. What the display saw waits for the program (plat/host/host.h's events).
 */
#ifndef PLAT_PGFX_H
#define PLAT_PGFX_H
#include "base.h"
#include "qx/qx.h"

/* A frame shown: w by h pixels of depth bits, w a row; at 8 bits its 256 colours, &HRRGGBB. */
typedef struct {
    const uint8_t *pixels;
    uint16_t w, h;
    uint8_t depth;
    const uint32_t *palette;
} PgfxFrame;

/* A GPU's screen: the shading languages it takes (1 << QxShader each), and whether clip space's y
   runs down a texture's rows; opened w by h with QxScreen's flags (QX_SCREEN_VSYNC its own if it
   shows at the retrace itself, the host waiting for the others), closed; a list played; w by h
   pixels at x, y of a texture read, bytes R, G, B and A; play true when the list showed one.
   Called from the interpreter's thread. */
typedef struct {
    uint8_t shaders;
    bool y_down;
    bool (*open)(uint16_t w, uint16_t h, uint8_t flags);
    void (*close)(void);
    bool (*play)(const uint32_t *list, uint32_t words);
    void (*read)(uint32_t texture, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *out);
    void (*hide)(bool hidden); /* the text screen shown in its place, or it again */
} PgfxPlayer;

/* A display's monitors and modes, as QxHost's gfx_monitors, gfx_modes and gfx_mode. Called from the
   interpreter's thread. */
typedef struct {
    uint32_t (*monitors)(void);
    uint32_t (*modes)(uint32_t monitor, QxMode *out, uint32_t most);
    bool (*mode)(uint32_t monitor, uint32_t n);
} PgfxDisplay;

void pgfx_set_player(const PgfxPlayer *p);
void pgfx_set_display(const PgfxDisplay *d);
/* A screen w by h the player plays, in the runtime's place, when it takes one of shaders: the
   player; null, none, or it could not. Open, a screen is closed first. */
const PgfxPlayer *pgfx_open_played(uint16_t w, uint16_t h, uint8_t shaders, uint8_t flags);
/* The open screen's player: null, none or pages. */
const PgfxPlayer *pgfx_playing(void);
/* A screen w by h, depth bits a pixel, in the runtime's place: false, none of that depth, or no
   memory. Open, it is closed first. */
bool pgfx_open(uint16_t w, uint16_t h, uint8_t depth, bool vsync);
void pgfx_close(void);
/* The open screen hidden, the text screen shown in its place (the editor's, at a stop), its page and
   what it draws kept: false, none open or hidden already. */
bool pgfx_hide(void);
/* The hidden screen shown again, its last frame: false, none hidden. */
bool pgfx_unhide(void);
/* The page to draw in, *pitch bytes a row: null, no screen open. */
uint8_t *pgfx_page(uint32_t *pitch);
void pgfx_show(const uint32_t *palette);
/* The list played on the open screen's player. */
void pgfx_play(const uint32_t *list, uint32_t words);
/* The display's refresh in Hz: 0, unknown or none. */
float pgfx_hz(void);
/* The frame shown last, while the screen is open: false, none. */
bool pgfx_shown(PgfxFrame *f);
/* The display's monitors: 0, none. */
uint32_t pgfx_monitors(void);
uint32_t pgfx_modes(uint32_t monitor, QxMode *out, uint32_t most);
/* As gfx_mode, while a screen is open: false, none open, no display, or it could not. */
bool pgfx_mode(uint32_t monitor, uint32_t n);
/* The next thing the display saw since the screen opened: false, none. */
bool pgfx_event(QxEvent *e);
#endif
