/* The debug pane: at a stop, right of the code (columns DP_COL on, the edit windows' rows), its
   windows Watch, Locals and Call stack (src/ui/dpane.asm).

   The frames: the stop's (vm.cont.line), then each procedure's caller, from the call frames, each with its text line and its row. Locals: the selected frame's procedure's
   variables, its parameters first, as that frame sees them (a newer call of the same procedure
   set them aside in its CallVars). Watch: expressions, each PRINTed
   by dbug_eval in the stop's procedure's scope at each stop; kept across runs. The tooltip:
   the Locals row of the variable under the mouse (uimn.tip_mouse) or the cursor. uimn.pane_on is
   ui.h's; the rest is dpan.c's. */
#ifndef UI_DPAN_H
#define UI_DPAN_H
#include "ui/ui.h"

/* dp_open: a stop: the pane shown, its frames, Locals, Watch's values; Watch's focus. */
void dpan_open(void);
/* dp_close: no pane; its focus to the edit window. */
void dpan_close(void);
/* dp_draw: the pane, when shown. */
void dpan_draw(void);
/* dp_key: key in the pane: Tab, Shift+Tab its windows; Up, Down a row; Enter a frame; Ins, Del
   in Watch a watch; Esc back to the edit window. */
void dpan_key(uint16_t key);
/* dp_mouse: the mouse event at row, col in the pane: a press focuses it, selects a row, a
   frame's row the frame. */
void dpan_mouse(uint8_t row, uint8_t col);
/* dp_hover: the mouse moved to row, col: the tooltip by it. */
void dpan_hover(uint8_t row, uint8_t col);
/* dp_tip: stopped, the program as it ran (uimn_stopped): the tooltip drawn by the name under
   the mouse or the cursor, in the selected frame's view. */
void dpan_tip(void);
#endif
