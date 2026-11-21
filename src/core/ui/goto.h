/* The Go To box's interface. How it works: goto.c. */
#ifndef UI_GOTO_H
#define UI_GOTO_H
#include "ui/ui.h"

/* The box's widget, a child of screen, hidden till goto_run. */
void goto_build(Widget *screen);
/* Ctrl+P: every procedure and file listed, narrowed by the letters typed; Enter shows the one
   chosen (a jump: jumps.h), Esc nothing. */
void goto_run(Session *S);
#endif
