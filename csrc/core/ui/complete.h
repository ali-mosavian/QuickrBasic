/* Tab completes the word at the cursor, from what fits there (src/ui/compl.asm; AC builds only).
   The line before the word tells what fits: a statement or a variable at a statement's start; a
   label after GOTO, GOSUB, THEN, RUN, LIST, ON's list; a variable after FOR, NEXT, DIM, READ,
   INPUT, GET; after an operand the words that go on; else a function or a variable of the type
   the expression wants. Its state, the list and its constants (AM_*, AP_*, AK_*, AT_*) are
   complete.c's. */
#ifndef UI_COMPLETE_H
#define UI_COMPLETE_H
#include "ui/ui.h"

#if AC
/* ac_tab: Tab in the program window: the word completed, or a list of what fits to choose from
   (its keys; another key leaves it and goes to uimn_key). false: nothing fits, Tab indents. */
bool cmpl_tab(Session *S);
#endif
#endif
