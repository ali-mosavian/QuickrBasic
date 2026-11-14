/* Window management's interface. How it works: window.c. */
#ifndef UI_WINDOW_H
#define UI_WINDOW_H
#include "ui/ui.h"

/* wn_init: one edit window (20 rows) over the immediate window (3), as QBasic starts. */
void wind_init(void);
/* wn_layout: each window's rows and columns (uimn.ed / win[].ed row0 rows x width,
   uimn.imm.row0 rows, its widget's rect, shown) from the heights, top down. */
void wind_layout(void);
/* wn_draw, by window (WidgetOps.draw): an edit window (the other than the active one with its
   state swapped in and its view applied), the immediate window; the pane's are its own. */
void wind_ed_draw(Widget *x, Session *S);
void wind_imm_draw(Widget *x, Session *S);
/* Window w's widget (W_ED1 .. W_DBG); the window of widget x, W_HELP none. */
Widget *wind_widget(uint8_t w);
uint8_t wind_of(const Widget *x);
/* wact: the active edit window, W_ED1 or W_ED2: the editors' focus. */
static inline uint8_t wind_active(void) { return (uint8_t)(W_ED1 + uimn.tree.editors.focus); }
/* wn_act: edit window w the active one, the focus in it (the line committed first). */
void wind_act(Session *S, uint8_t w);
/* wn_focus: window w has the focus: an edit window active, the immediate window, the pane. */
void wind_focus(Session *S, uint8_t w);
/* wn_cur: the window with the focus. */
uint8_t wind_cur(void);
/* wn_next, wn_prev: F6, Shift+F6: the focus to the next window shown (ED1 ED2 pane IMM), the
   one before. */
void wind_next(Session *S);
void wind_prev(Session *S);
/* wn_at: the window at row, col; false: none (the menu or status bar, a hidden one's). */
bool wind_at(uint8_t row, uint8_t col, uint8_t *w);
/* wn_split: View, Split: the edit window in two, the same text in both; again, one. */
void wind_split(Session *S);
/* wn_vsplit: View, Split Vertically: side by side; again, one. */
void wind_vsplit(Session *S);
/* wn_grow, wn_shrink: Alt+Plus, Alt+Minus: the focused window a row taller, from the one under
   it, else over it; a row shorter, to the same. */
void wind_grow(void);
void wind_shrink(void);
/* wn_max: Ctrl+F10: the focused window the whole screen; again, the windows as they were. */
void wind_max(void);
#endif
