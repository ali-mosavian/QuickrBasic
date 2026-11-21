/* What matches the cursor's bracket or block keyword. How it works: pairs.c. */
#ifndef UI_PAIRS_H
#define UI_PAIRS_H
#include "ui/ui.h"

/* The matches of the cursor's place found again if it or the text changed: the active window's,
   before it draws. */
void pairs_find(Session *S);
/* Text line line (the whole text's) on screen row row from column col: its matches drawn in
   SL_MATCH. */
void pairs_paint(uint8_t row, uint8_t col, uint32_t line);
/* Ctrl+]: the cursor to the block's next keyword, round, or the partner bracket; a beep for
   none. */
void pairs_jump(Session *S);
#endif
