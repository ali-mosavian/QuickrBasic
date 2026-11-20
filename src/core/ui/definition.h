/* Go to Definition's interface. How it works: definition.c. */
#ifndef UI_DEFINITION_H
#define UI_DEFINITION_H
#include "ui/ui.h"

/* Ctrl+B, Ctrl+click: the line that defines the name at the program window's cursor shown, the
   cursor on the name there; none, a beep. */
void defn_goto(Session *S);
#endif
