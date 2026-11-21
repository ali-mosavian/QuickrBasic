/* Whole lines moved, copied and commented out. How it works: lines.c. */
#ifndef UI_LINES_H
#define UI_LINES_H
#include "ui/ui.h"

/* Alt+Up, Alt+Down: the cursor's line, or the lines the selection covers, swapped with the one
   above or below; the cursor and selection with them. */
void lines_move(Session *S, bool down);
/* Shift+Alt+Up, Shift+Alt+Down: them copied, the cursor on the copy above or below. */
void lines_copy(Session *S, bool down);
/* Ctrl+/, Ctrl+-: "' " put before each one's statement, or taken away when every one has it. */
void lines_comment(Session *S);
#endif
