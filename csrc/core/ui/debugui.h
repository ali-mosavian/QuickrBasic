/* The debugger UI's interface. How it works: debugui.c. */
#ifndef UI_DEBUGUI_H
#define UI_DEBUGUI_H
#include "ui/ui.h"

/* ud_init: the session's debugger on, its trace hook ours. */
void udbg_init(Session *S);
/* ud_prep: before a run or CONT: the pane closed, nothing lit, uimn.stopped off, breakpoints
   on, Ctrl+F7's line the until line. */
void udbg_prep(Session *S);
/* ud_stop: a stop CONT goes on from: uimn.stopped, the debug pane, its statement lit, the cursor
   at it. None: nothing. */
void udbg_stop(Session *S);
/* ud_row: the edit window's row at row, col shows view line line: a breakpoint's colour, the lit
   statement's (not while that line is being changed). */
void udbg_row(Session *S, uint8_t row, uint8_t col, uint32_t line);

/* The Debug menu's commands and the run keys. */
/* ud_step: Step, F8: the next statement, into a procedure. */
void udbg_step(Session *S);
/* ud_pstep: Step Over, F10 (and F7 as the status bar's Over): the next statement at this depth
   or less. */
void udbg_pstep(Session *S);
/* ud_sout: Step Out, Shift+F8: on till the procedure or GOSUB stopped in has returned. */
void udbg_sout(Session *S);
/* ud_restart: Run, Restart: from the start, stopped at its first statement. */
void udbg_restart(Session *S);
/* ud_trace1: Trace On: on, off. */
void udbg_tron(Session *S);
/* ud_bp: Toggle Breakpoint, F9: the cursor's line (the program window's only). */
void udbg_bp(Session *S);
/* ud_bpclr: Clear All Breakpoints. */
void udbg_bpclear(Session *S);
/* ud_until: Ctrl+F7: on to the cursor's line (the program window's only; else nothing). */
void udbg_until(Session *S);
/* ud_setnx: Set Next Statement: CONT goes on at the cursor's line, in the procedure stopped in;
   else a box (cannot continue; another procedure's line). */
void udbg_setnext(Session *S);
#endif
