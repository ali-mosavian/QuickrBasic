/* The debug pane's interface. How it works: debugpane.c. */
#ifndef UI_DEBUGPANE_H
#define UI_DEBUGPANE_H
#include "ui/ui.h"

/* dp_open: a stop: the pane shown, its frames, Locals, Watch's values; Watch's focus. */
void dpan_open(Session *S);
/* dp_close: no pane; its focus to the edit window. */
void dpan_close(void);
/* The pane's widgets: its ops and children kids, Watch, Locals, Call stack, the tooltip. Keys:
   Tab, Shift+Tab its windows; Up, Down a row; Enter a frame; Ins, Del in Watch a watch; Esc back
   to the edit window. A press focuses a window, selects a row, a frame's row the frame. */
enum { DPAN_KIDS = 4 };
void dpan_build(Widget *pane, Widget kids[static DPAN_KIDS]);
/* dp_geo: its windows' rects, from the rows the immediate window leaves. */
void dpan_layout(void);
/* dp_hover: the mouse moved to row, col: the tooltip by it. */
void dpan_hover(uint8_t row, uint8_t col);
/* dp_tip: stopped, the program as it ran (uimn_stopped): the tooltip drawn by the name under
   the mouse or the cursor, in the selected frame's view. */
void dpan_tip(Session *S);
#endif
