/* Values as the debugger shows them. How it works: debugvalue.c. */
#ifndef UI_DEBUGVALUE_H
#define UI_DEBUGVALUE_H
#include "ui/ui.h"

/* A row of the debug pane being made: an expression, " = ", a value or a message. */
enum { DVAL_ROW = 256 };

/* s at d, its 0 left out; past it. */
char *dval_cpy(char *d, const char *s);
/* v in decimal at d, a sign when negative; past it. */
char *dval_dec(char *d, int32_t v);
/* s to d, cut to DP_TWMAX - 1 characters, 0-ended. */
void dval_cut(char *d, const char *s);
/* Variable id's value in cell as text at d: a number as PRINT's without its sign's space, a string
   in quotes. Past it. */
char *dval_var(Session *S, char *d, uint32_t id, Addr cell);
#endif
