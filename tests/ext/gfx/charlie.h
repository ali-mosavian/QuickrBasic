/* A frame of CHARLIE for gfx's tests and the Mac's window: the game's pictures in CHARLIE.BMP, cut as
 * barkley/assets/sheet.bas cuts them (tools/charlieart.py writes both), drawn in steps: the backdrop,
 * the hills, a row of ledge tiles, a tree, Charlie and his mirror, a bone, the title, and a line of
 * the built-in font. Paths are from the repository's top.
 */
#ifndef GFX_TEST_CHARLIE_H
#define GFX_TEST_CHARLIE_H
#include "call.h"

#define CHARLIE_ATLAS "src/plat/wasm/web/demos/CHARLIE.BMP"
#define CHARLIE_SHEET "src/plat/wasm/web/demos/barkley/assets/sheet.bas"

enum { CHARLIE_W = 320, CHARLIE_H = 200, CHARLIE_CUTS = 4096 };

typedef enum : uint8_t { CHARLIE_SPRITE, CHARLIE_TILES, CHARLIE_TEXT } CharlieKind;

/* A sprite of picture at x, y; count of its cells from x, y; text in colour. */
typedef struct {
    CharlieKind kind;
    int32_t picture;
    int32_t x, y, flags, count, colour;
    const char *text;
} CharlieStep;

/* A picture's place in the atlas: w 0, none. */
typedef struct {
    int32_t x, y, w, h;
} CharlieCut;

extern const CharlieStep charlie_steps[];
extern const uint32_t charlie_nsteps;

/* sheet.bas's cuts at path, by picture, n places: false, not read. */
bool charlie_cuts(const char *path, CharlieCut *cuts, uint32_t n);
/* The frame drawn by gfx on host, not yet flipped: the canvas opened, the atlas loaded and cut, its
   colours row 0's. Each step's view in views (by step). The first error, 0 none. */
uint8_t charlie_draw(const QxHost *host, const CharlieCut *cuts, int32_t *views);
#endif
