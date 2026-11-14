/* Compile errors in words: the interface. How it works: diag.c. */
#ifndef CORE_IO_DIAG_H
#define CORE_IO_DIAG_H
#include "defs.h"
#include "io/msg.h"

enum { DX_NOSTATE = 0xFFFF }; /* an error's automaton state: none */

/* Place error code (E_*), dg (DG_*), met at column col of the 0-ended text, state the statement
   automaton's state it failed in; the column of the text it is about. */
uint32_t diag_at(
    Session *S,
    const char *text,
    uint32_t col,
    uint8_t code,
    uint8_t dg,
    uint16_t state
);

/* An error found off the code (the block pass), raised as if an ERR op at body offset at of
   line (a program store offset) raised it, never trapped; other its {other} text, len bytes
   (31 at most kept). vm.opip is set to an ErrOp (vm/exec.h) of diag's own. */
[[noreturn]]
void diag_raise(
    Session *S,
    uint8_t code,
    uint8_t dg,
    uint32_t at,
    uint32_t line,
    const char *other,
    uint32_t len
);

/* The error diag_at placed: what is wrong, what to do, the form indented; a line each. */
void diag_say(Session *S);
/* What is wrong, a sentence. */
void diag_what(Session *S);
/* What to do, a sentence; false: no words for it, nothing printed. */
bool diag_todo(Session *S);
/* How the statement is written; false: not known, nothing printed. */
bool diag_shape(Session *S);
/* diag's state: the line being diagnosed and what it says */
typedef struct {
    const char *line; /* the line's text */
    uint16_t state;   /* the automaton's state, FFFFh none */
    uint8_t code;     /* the error, E_* */
    uint8_t dg;       /* its DG_, 0 none */
    uint32_t col;     /* the text's column */
    uint32_t len;     /* its length */
    bool first;       /* at the statement's first word */
    const char *name; /* the name, its length */
    uint32_t namelen;
    const char *what; /* what is wrong, its words */
    const char *todo; /* what to do, nullptr none */
    const char *bc;   /* its name, when no words */
    char *keyend;     /* the key's end before " /" */
    char stm[16];     /* the statement's word, STM_MAX letters at most */
    char key[160];    /* a key, a word; a form's line */
    char other[32];   /* {other} */
    const char *form; /* the statement's form, nullptr none */
    bool cap;         /* fill: a capital next */
    char last;        /* fill: the last it put */
} Diag;

#endif
