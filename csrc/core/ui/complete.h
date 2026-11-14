/* Tab completion's interface. How it works: complete.c. */
#ifndef UI_COMPLETE_H
#define UI_COMPLETE_H
#include "ui/ui.h"

#if AC
/* ac_tab: Tab in the program window: the word completed, or a list of what fits to choose from
   (its keys; another key leaves it and goes to uimn_key). false: nothing fits, Tab indents. */
bool cmpl_tab(Session *S);
#endif
#endif
