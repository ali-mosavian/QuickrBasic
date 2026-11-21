/* The debugger's Locals. How it works: debuglocals.c. */
#ifndef UI_DEBUGLOCALS_H
#define UI_DEBUGLOCALS_H
#include "ui/debugstack.h"
#include "vm/inspect.h"

/* A new stop: a change seen from now on is this stop's. */
void dloc_stop(void);
/* Locals of the stop's frame f (debugstack.h's): its procedure's variables as it sees them, then
   the module's it sees, SHARED; named as declared, the records and arrays opened under theirs. */
void dloc_read(Session *S, uint8_t f);
/* The rows read; row i of them. */
uint8_t dloc_count(void);
const char *dloc_row(uint8_t i);
/* Row i's value changed at this stop: its first column + 1; else 0. */
uint8_t dloc_changed(uint8_t i);
/* Row i's value, nullptr none (a line, an array's rest); its expression, as a program writes it;
   its text's width, uncut: past a window's, it does not all show there. */
const InspValue *dloc_value(uint8_t i);
const char *dloc_path(uint8_t i);
uint16_t dloc_width(uint8_t i);
/* Row i opened: a record's fields under it, an array's elements, the rest of an array's next ones;
   false it opens not. */
bool dloc_open(Session *S, uint8_t i);
/* Row i's record or array shut, else the one it is under; that row's index, i none. */
uint8_t dloc_close(Session *S, uint8_t i);
/* The row of the variable named s, n long, suffix sfx (0 none); DLOC_NONE none. */
enum : uint8_t { DLOC_NONE = UINT8_MAX };
uint8_t dloc_find(Session *S, const uint8_t *s, uint32_t n, uint8_t sfx);
#endif
