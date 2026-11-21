/* Closing pairs as they are typed. How it works: closers.c. */
#ifndef UI_CLOSERS_H
#define UI_CLOSERS_H
#include "ui/ui.h"

/* ch typed in the cursor's line: its closing one put in after it, or ch stepped over. false:
   neither; ch is typed as any. */
bool clos_type(Session *S, uint8_t ch);
/* Backspace between an empty pair: both out. false: not between one. */
bool clos_bs(Session *S);
#endif
