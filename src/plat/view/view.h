/* A display of the screen, the browser's or a window's: the screen as RGBA, the runtime's text on
 * pscr's pages, the program's graphics kept while the editor shows. Only plat/view, its displays and
 * the runtime's glue include it.
 */
#ifndef PLAT_VIEW_H
#define PLAT_VIEW_H
#include "base.h"

enum { VIEW_MAX_W = 720, VIEW_MAX_H = 480, VIEW_CELL_W = 9 };

/* The page's layout is wasm.mjs's too. */
typedef struct {
    uint32_t width, height;
    uint32_t cell_h; /* a text cell's height, for a pointer; 0 in a graphics mode */
    uint32_t pad;
    uint32_t rgba[VIEW_MAX_W * VIEW_MAX_H];
} ViewFrame;

/* The mouse's text cursor, drawn while shown. */
typedef struct {
    bool shown;
    uint8_t row, col;
} ViewPointer;

/* The screen as a VGA card shows it (the page shown, or the program's graphics), in RGBA (bytes R,
   G, B, A); blink is the blink's on phase (and the cursor's). */
const ViewFrame *view_frame(bool blink, ViewPointer pointer);

/* The editor is shown over the program's graphics (qbrt_shot_take), until the output is again. */
extern bool view_text_shown;
#endif
