/* A compile error in words, for the person who wrote the line (io/diag.asm, from llrm's
   front/explain.rs). An error is its E_*, its DG_ (io/msg.h; 0: found by BC's name and the text
   at the error) and its column in the line's text. diag_at reads the place; diag_say prints what
   is wrong, what to do, and the statement's form. */
#ifndef CORE_IO_DIAG_H
#define CORE_IO_DIAG_H
#include "defs.h"
#include "io/msg.h"

enum { DX_NOSTATE = 0xFFFF }; /* an error's automaton state: none */

/* Place error code (E_*), dg (DG_*), met at column col of the 0-ended text, state the statement
   automaton's state it failed in; the column of the text it is about. */
uint32_t diag_at(const char *text, uint32_t col, uint8_t code, uint8_t dg, uint16_t state);

/* An error found off the code (the block pass), raised as if an ERR op at body offset at of
   line (a program store offset) raised it, never trapped; other its {other} text, len bytes
   (31 at most kept). vm.opip is set to an ErrOp (vm/exec.h) of diag's own. */
[[noreturn]] void diag_raise(uint8_t code, uint8_t dg, uint32_t at, uint32_t line,
                             const char *other, uint32_t len);

/* The error diag_at placed: what is wrong, what to do, the form indented; a line each. */
void diag_say(void);
/* What is wrong, a sentence. */
void diag_what(void);
/* What to do, a sentence; false: no words for it, nothing printed. */
bool diag_todo(void);
/* How the statement is written; false: not known, nothing printed. */
bool diag_shape(void);
#endif
