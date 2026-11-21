/* The debugger's Locals. How it works: debuglocals.c. */
#ifndef UI_DEBUGLOCALS_H
#define UI_DEBUGLOCALS_H
#include "ui/debugstack.h"

/* A new stop: a change seen from now on is this stop's. */
void dloc_stop(void);
/* Locals of the stop's frame f (debugstack.h's): its procedure's variables as it sees them, named
   as declared. */
void dloc_read(Session *S, uint8_t f);
/* The rows read; row i of them. */
uint8_t dloc_count(void);
const char *dloc_row(uint8_t i);
/* Row i's value changed at this stop: its first column + 1; else 0. */
uint8_t dloc_changed(uint8_t i);
/* The row of the name s, n long, suffix sfx (0 none): a Locals variable of it, else a module one
   the frame's procedure sees, SHARED; nullptr none. */
const char *dloc_find(Session *S, const uint8_t *s, uint32_t n, uint8_t sfx);
#endif
