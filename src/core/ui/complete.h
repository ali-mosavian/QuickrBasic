/* Tab completion's interface. How it works: complete.c. */
#ifndef UI_COMPLETE_H
#define UI_COMPLETE_H
#include "ui/ui.h"

#if AC
/* ac_tab: Tab in the program window: the word completed, or a list of what fits to choose from
   (its keys; another key leaves it and goes to uimn_key). false: nothing fits, Tab indents. */
bool cmpl_tab(Session *S);
/* A character typed at the program window's line end, no key waiting: the likeliest word's rest
   kept to show dim past the cursor; Tab takes it. */
void cmpl_typed(Session *S);
/* That rest, while the line is as it was. */
void cmpl_draw_ghost(void);
#endif
#endif
