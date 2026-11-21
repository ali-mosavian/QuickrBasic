/* The debugger's Locals. How it works: debuglocals.c. */
#ifndef UI_DEBUGLOCALS_H
#define UI_DEBUGLOCALS_H
#include "ui/debugstack.h"

/* A new stop: a change seen from now on is this stop's. */
void dloc_stop(void);
/* Locals of frame f: its procedure's variables as it sees them, named as declared. newest: f is its
   procedure's newest frame (dstk_newest), whose values are the ones a change is seen in. */
void dloc_read(Session *S, const StackFrame *f, bool newest);
/* The rows read; row i of them. */
uint8_t dloc_count(void);
const char *dloc_row(uint8_t i);
/* The row of the name s, n long, suffix sfx (0 none): a Locals variable of it, else a module one
   the frame's procedure sees, SHARED; nullptr none. */
const char *dloc_find(Session *S, const uint8_t *s, uint32_t n, uint8_t sfx);
#endif
