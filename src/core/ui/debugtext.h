/* The debug pane's rows' text. How it works: debugtext.c. */
#ifndef UI_DEBUGTEXT_H
#define UI_DEBUGTEXT_H
#include "ui/ui.h"

/* A row being made: an expression, " = ", a value or a message. */
enum { DTXT_ROW = 256 };

/* s at d, its 0 left out; past it. */
char *dtxt_cpy(char *d, const char *s);
/* v in decimal at d, a sign when negative; past it. */
char *dtxt_dec(char *d, int32_t v);
/* s to d, cut to DP_TWMAX - 1 characters, 0-ended. */
void dtxt_cut(char *d, const char *s);
#endif
