/* The windows: their rows, focus, Split, grow, shrink, maximise (src/ui/win.asm).

   They stack from row 1 to row 23: W_ED1, W_ED2 (Split), W_IMM, each uimn.win.win[w].height rows
   with its title, 0 hidden. uimn.win.vert: ED2 beside ED1, in its rows, each half the columns.
   An edit window has a bottom row, the sideways scroll bar's with the focus. The active edit
   window's state is uimn.ed and uimn.edline.col; the other's waits in its win[].ed, win[].col.
   With the debug pane (uimn.pane_on) the edit windows have DP_COL columns, the pane the rest.
   A window is W_ED1, W_ED2, W_IMM, or W_DBG for the pane. */
#ifndef UI_WIND_H
#define UI_WIND_H
#include "ui/ui.h"

/* wn_init: one edit window (20 rows) over the immediate window (3), as QBasic starts. */
void wind_init(void);
/* wn_layout: each window's rows and columns (uimn.ed / win[].ed row0 rows x width,
   uimn.imm.row0 rows) from the heights, top down. */
void wind_layout(void);
/* wn_draw: each window shown: the edit windows (the other with its state swapped in and its
   view applied), the pane, the immediate window. */
void wind_draw(void);
/* wn_act: edit window w the active one, the focus in it (the line committed first). */
void wind_act(uint8_t w);
/* wn_focus: window w has the focus: an edit window active, the immediate window, the pane. */
void wind_focus(uint8_t w);
/* wn_cur: the window with the focus. */
uint8_t wind_cur(void);
/* wn_next, wn_prev: F6, Shift+F6: the focus to the next window shown (ED1 ED2 pane IMM), the
   one before. */
void wind_next(void);
void wind_prev(void);
/* wn_at: the window at row, col; false: none (the menu or status bar, a hidden one's). */
bool wind_at(uint8_t row, uint8_t col, uint8_t *w);
/* wn_split: View, Split: the edit window in two, the same text in both; again, one. */
void wind_split(void);
/* wn_vsplit: View, Split Vertically: side by side; again, one. */
void wind_vsplit(void);
/* wn_grow, wn_shrink: Alt+Plus, Alt+Minus: the focused window a row taller, from the one under
   it, else over it; a row shorter, to the same. */
void wind_grow(void);
void wind_shrink(void);
/* wn_max: Ctrl+F10: the focused window the whole screen; again, the windows as they were. */
void wind_max(void);
#endif
