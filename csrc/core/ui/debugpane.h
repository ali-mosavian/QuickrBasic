/* The debug pane's interface. How it works: debugpane.c. */
#ifndef UI_DEBUGPANE_H
#define UI_DEBUGPANE_H
#include "ui/ui.h"

/* dp_open: a stop: the pane shown, its frames, Locals, Watch's values; Watch's focus. */
void dpan_open(Session *S);
/* dp_close: no pane; its focus to the edit window. */
void dpan_close(void);
/* dp_draw: the pane, when shown. */
void dpan_draw(void);
/* dp_key: key in the pane: Tab, Shift+Tab its windows; Up, Down a row; Enter a frame; Ins, Del
   in Watch a watch; Esc back to the edit window. */
void dpan_key(Session *S, uint16_t key);
/* dp_mouse: the mouse event at row, col in the pane: a press focuses it, selects a row, a
   frame's row the frame. */
void dpan_mouse(Session *S, uint8_t row, uint8_t col);
/* dp_hover: the mouse moved to row, col: the tooltip by it. */
void dpan_hover(uint8_t row, uint8_t col);
/* dp_tip: stopped, the program as it ran (uimn_stopped): the tooltip drawn by the name under
   the mouse or the cursor, in the selected frame's view. */
void dpan_tip(Session *S);
#endif
