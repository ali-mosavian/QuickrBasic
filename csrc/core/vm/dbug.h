/* The debugger's VM side (src/vm/dbg.asm): where a run stops, TRON, TROFF, the watch probe.

   With dbug.emit set (the IDE) each program statement's code starts with a DBG op. It stops for a
   step (mode), a breakpoint at its line's first statement (LF_BP), and until's line (once). A
   stop is a STOP: CONT goes on past the DBG. With trace set each statement first goes to hook,
   its place in wline, stmt_from, stmt_to. The direct line has no DBG. */
#ifndef CORE_VM_DBUG_H
#define CORE_VM_DBUG_H
#include "vm/exec.h"

typedef struct {
    uint8_t mode;          /* DM_*: the stop the next DBG looks for */
    bool trace;            /* each statement to hook (TRON) */
    bool bpon;             /* a line's LF_BP stops */
    bool until;            /* utl: a one-time stop at line uline */
    bool emit;             /* a DBG op starts each statement (comp's) */
    uint32_t pdep;         /* DM_PSTEP: stop at this depth or less */
    uint32_t uline;
    void (*hook)(void);    /* trace's routine (ui/dbg's), nullptr none */
    uint32_t wline;        /* dbug_pos's line, */
    uint32_t stmt_from;    /*   its statement's body offsets, from */
    uint32_t stmt_to;      /*   and to (BOFF the line's end) */
    bool probing;          /* g_dbev: dbug_eval's line runs */
    jmp_buf probe;         /* dbug_eval's, while its line runs */
} Dbug;
extern Dbug dbug;

/* dbg_depth: the procedures and GOSUBs being run. */
uint32_t dbug_depth(const Vm *vm);
/* dbg_pos: the place of code in line to dbug.wline, stmt_from, stmt_to. */
void dbug_pos(uint32_t line, const uint8_t *code);
/* dbg_eval: PRINT expr in procedure proc's scope (0 the module), its output to out, 0-ended,
   79 chars at most; the stopped run as it was. A FUNCTION it calls runs; no breakpoint, step,
   trace or event stops it. 0, else the error (E_*) + 1. */
uint8_t dbug_eval(const char *expr, uint8_t proc, char out[static 80]);
/* dbg_evx: the probe's line ended (sc_end, ex_end): back to dbug_eval. */
[[noreturn]] void dbug_done(void);
/* dbg_ever: the probe's line failed with code (repl_error): back to dbug_eval. */
[[noreturn]] void dbug_fail(uint8_t code);
#endif
