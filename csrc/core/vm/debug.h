/* The debugger's interface. How it works: debug.c. */
#ifndef CORE_VM_DBUG_H
#define CORE_VM_DBUG_H
#include "vm/exec.h"

/* A statement's place: its program line, its body offsets from and to (BOFF the line's end). */
typedef struct {
    uint32_t line, from, to;
} DebugPos;

/* What the VM reports to the front end, nullptr: nothing. */
typedef struct {
    void (*trace)(Session *S, const DebugPos *at); /* a statement starts under TRON */
} DebugHooks;

typedef struct Dbug {
    uint8_t mode;  /* DM_*: the stop the next DBG looks for */
    bool trace;    /* each statement to hook (TRON) */
    bool bpon;     /* a line's LF_BP stops */
    bool until;    /* utl: a one-time stop at line uline */
    bool emit;     /* a DBG op starts each statement (comp's) */
    uint32_t pdep; /* DM_PSTEP: stop at this depth or less */
    uint32_t uline;
    const DebugHooks *hooks; /* nullptr none */
    bool probing;            /* g_dbev: dbug_eval's line runs, under its catch */
} Dbug;

/* dbg_depth: the procedures and GOSUBs being run. */
uint32_t dbug_depth(const Vm *vm);
/* dbg_pos: the place of code in line. */
DebugPos dbug_pos(Session *S, uint32_t line, const uint8_t *code);
/* dbg_eval: PRINT expr in procedure proc's scope (0 the module), its output to out, 0-ended,
   79 chars at most; the stopped run as it was. A FUNCTION it calls runs; no breakpoint, step,
   trace or event stops it. Its error, else ST_OK. */
Status dbug_eval(Session *S, const char *expr, uint8_t proc, char out[static 80]);
/* dbg_evx: the probe's line ended (sc_end, ex_end): back to dbug_eval, ST_OK. An error comes
   back through its catch (dbg_ever). */
[[noreturn]]
void dbug_done(Session *S);
#endif
