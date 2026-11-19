/* The drawing's interface. How it works: draw.c. */
#ifndef UI_DRAW_H
#define UI_DRAW_H
#include "ui/ui.h"

/* dr_all: the whole screen (the widget tree, uimn.screen) and the cursor in the
   focused window, its shape the mode's (hidden in the pane). */
void draw_all(Session *S);
/* dr_edit: the edit window in uimn.ed: its frame and title, its lines (synt_row, udbg_row), the
   selection, its scroll bars when it has the focus. inactive (dr_ina): the window drawn is not
   the active one (wind_ed_draw swapped its state in): no selection, no bars, uimn.edline not its. */
void draw_edit(Session *S, bool inactive);
/* The line edit window e (ED1) shares with ED2 beside it, over ED2's border: a joint at the
   top; bar: e's scroll bar on it (e has the focus). */
void draw_seam(const UiEdit *e, bool bar);
/* dr_imm: the immediate window: its title, its lines; im_top kept so that im_i shows. */
void draw_imm(void);
/* The cursor (WidgetOps.cursor) in the edit window that has the focus, in the immediate window. */
void draw_edit_cursor(Widget *w);
void draw_imm_cursor(Widget *w);
/* dr_status: the status bar: the keys (draw_keys) or uimn.help after "F1=Help"; the cursor's
   line and column from column 62 when they fit, ^Q pending, Caps and Num Lock, as QBasic's. */
void draw_status(void);
/* dr_skeys: the status bar's keys for the state: the immediate window's, else Continue's (a stop
   to go on from: uimn_stopped), else the editor's. */
const StatusKeys *draw_keys(void);
/* dr_ttl: a window's title row: from col, width wide; left, right its corners; title centred,
   lit (SL_TTL) when lit; arrow: the glyph before the right corner, maximise or restore, 0 none. ttlx, ttlw were the
   asm's globals for col, width. */
void draw_title(
    uint8_t row,
    uint8_t col,
    uint8_t width,
    uint8_t left,
    uint8_t right,
    const char *title,
    bool lit,
    uint8_t arrow
);
/* draw_title with its line, and its title unlit, in slot. */
void draw_title_in(
    uint8_t slot,
    uint8_t row,
    uint8_t col,
    uint8_t width,
    uint8_t left,
    uint8_t right,
    const char *title,
    bool lit,
    uint8_t arrow,
    uint8_t lit_slot /* the title when lit */
);
#endif
