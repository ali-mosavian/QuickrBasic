/* The statements' CALL routines (src/vm/calls.asm): PRINT, INPUT, READ and DATA, DIM, RUN, LIST,
   CONT, END, STOP, POKE, WAIT, INP, OUT, SYS, ^. The rest are K handlers (vm/ops.def). */
#ifndef CORE_VM_STMT_H
#define CORE_VM_STMT_H
#include "vm/exec.h"

/* READ's buffer carved (g_rdbuf: a READ item's temporary points into it). */
void stmt_init(void);
/* ex_runfirst: run from the first line (END at once with none); vm.ip set, files closed. */
void stmt_runfirst(void);
/* ex_rungo: both stacks emptied, events, COMMON and ON ERROR cleared; on at line. */
void stmt_rungo(uint32_t line);
/* rd_first: the next READ starts at the first DATA. */
void stmt_restore(void);
/* sc_cont, K_CONT: on where the run stopped. Can't continue: no place. */
void stmt_cont(Vm *vm);
/* sc_end, K_END: CONT goes on after it; READY. */
[[noreturn]] void stmt_end(Vm *vm);
/* sc_stop, K_STOP: CONT goes on at vm.ip; BREAK. */
[[noreturn]] void stmt_stop(Vm *vm);
#endif
