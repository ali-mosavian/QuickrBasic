/* The places jumped from. How it works: jumps.c. */
#ifndef UI_JUMPS_H
#define UI_JUMPS_H
#include "ui/ui.h"

/* Places kept from now, moved with the lines added and taken out (uimn.text.edits, after the
   hook already set). */
void jumps_init(void);
/* None kept: a new or opened program. */
void jumps_clear(void);
/* A jump starts: the cursor's place kept to come back to; none to go forward to. */
void jumps_push(void);
/* Alt+Left, Alt+Right: back to the last place kept, forward to the one left; a beep for none. */
void jumps_back(Session *S);
void jumps_forward(Session *S);
#endif
