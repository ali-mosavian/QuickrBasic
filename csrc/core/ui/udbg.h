/* The debugger's UI: the Debug menu, F7 to F10, the lit statement (src/ui/dbg.asm).

   It drives vm's dbug (vm/dbug.h). udbg_init sets dbug.emit (every program statement starts
   with a DBG op) and dbug.hook (the trace's routine, udbg.c's: the statement lit on the editor's
   page for two ticks, then the output's page again). A breakpoint is a text line's mark, its
   record's LF_BP (text.h), so it moves with the line. A step, a breakpoint or until's line stops
   the run as STOP does: repl_error(E_BRK) → uimn_error(E_BRK) → udbg_stop. The stopped
   statement is lit: found by tokenizing its line again, lexr.col giving the column of dbug's
   statement body offsets. */
#ifndef UI_UDBG_H
#define UI_UDBG_H
#include "ui/ui.h"

/* ud_init: dbug.emit, dbug.hook. */
void udbg_init(void);
/* ud_prep: before a run or CONT: the pane closed, nothing lit, uimn.stopped off, breakpoints
   on, Ctrl+F7's line to dbug (until, uline). */
void udbg_prep(void);
/* ud_stop: a stop CONT goes on from (exec.cont_line): uimn.stopped, the debug pane, its
   statement lit, the cursor at it. No cont_line: nothing. */
void udbg_stop(void);
/* ud_row: the edit window's row at row, col shows view line line: a breakpoint's colour, the lit
   statement's (not while that line is being changed). */
void udbg_row(uint8_t row, uint8_t col, uint32_t line);

/* The Debug menu's commands and the run keys. */
/* ud_step: Step, F8: the next statement, into a procedure. */
[[noreturn]] void udbg_step(void);
/* ud_pstep: Step Over, F10 (and F7 as the status bar's Over): the next statement at this depth
   or less. */
[[noreturn]] void udbg_pstep(void);
/* ud_sout: Step Out, Shift+F8: on till the procedure or GOSUB stopped in has returned. */
[[noreturn]] void udbg_sout(void);
/* ud_restart: Run, Restart: from the start, stopped at its first statement. */
[[noreturn]] void udbg_restart(void);
/* ud_trace1: Trace On: dbug.trace on, off. */
void udbg_tron(void);
/* ud_bp: Toggle Breakpoint, F9: the cursor's line (the program window's only). */
void udbg_bp(void);
/* ud_bpclr: Clear All Breakpoints. */
void udbg_bpclear(void);
/* ud_until: Ctrl+F7: on to the cursor's line (the program window's only; else nothing). */
void udbg_until(void);
/* ud_setnx: Set Next Statement: CONT goes on at the cursor's line, in the procedure stopped in;
   else a box (cannot continue; another procedure's line). */
void udbg_setnext(void);
#endif
