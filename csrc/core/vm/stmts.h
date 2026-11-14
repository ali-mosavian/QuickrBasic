/* The statements' CALL routines (src/vm/calls.asm): PRINT, INPUT, READ and DATA, DIM, RUN, LIST,
   CONT, END, STOP, POKE, WAIT, INP, OUT, SYS, ^. The rest are K handlers (vm/ops.def). */
#ifndef CORE_VM_STMT_H
#define CORE_VM_STMT_H
#include "vm/exec.h"

/* READ's buffer carved (g_rdbuf: a READ item's temporary points into it). */
void stmt_init(Session *S);
/* ex_runfirst: run from the first line (END at once with none); vm.ip set, files closed. */
void stmt_runfirst(Session *S);
/* ex_rungo: both stacks emptied, events, COMMON and ON ERROR cleared; on at line. */
void stmt_rungo(Session *S, uint32_t line);
/* rd_first: the next READ starts at the first DATA. */
void stmt_restore(Session *S);
/* sc_cont, K_CONT: on where the run stopped. Can't continue: no place. */
void stmt_cont(Session *S);
/* sc_end, K_END: CONT goes on after it; READY. */
[[noreturn]]
void stmt_end(Session *S);
/* sc_stop, K_STOP: CONT goes on at vm.ip; BREAK. */
[[noreturn]]
void stmt_stop(Session *S);
/* READ's and INPUT's state */
typedef struct {
    uint8_t *data_text;      /* g_rdbuf: the DATA in hand's text, in the arena */
    const uint8_t *data_pos; /* READ's place in data_text, null before the first READ */
    uint32_t data_at;        /* the DATA's first byte in the store, 0 none */
    bool before_first;       /* data_pos is before the DATA's first item */
    uint8_t *input_end;      /* where an INPUT's empty line goes on */
    uint8_t *input_start;    /* REDO comes back here */
    bool redo;               /* a bad field: REDO FROM START after the stores */
} StmtState;

#endif
