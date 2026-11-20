/* The debugger's VM side: where a run stops, TRON, TROFF and the watch probe.
 *
 * # How it works
 * With dbug.emit set (the IDE) each program statement's code starts with a DBG op (dbug_dbg). It
 * stops for a step (mode), a breakpoint at its line's first statement (LF_BP) and until's line
 * (once). A stop is a STOP: CONT goes on past the DBG. With trace set each statement first goes to
 * hooks->trace with its place. The direct line has no DBG.
 *
 * The probe (dbug_eval) runs PRINT expr in a procedure's scope as a nested exec_run, the stopped
 * run's fields saved and restored around it. A FUNCTION it calls runs; no breakpoint, step, trace
 * or event stops it. Its end (dbug_done) or error (dbug_fail) longjmps back to dbug_eval.
 *
 * # asm structure (retired/asm32/vm/dbg.asm)
 * | asm | here | |
 * |---|---|---|
 * | TDBG, g_state.dbg | Dbug | MIRROR |
 * | dbg.mode, trace, bpon, utl, emit, pdep, uline, hook | Dbug.mode, trace, bpon, until, emit, pdep, uline, hooks | MIRROR |
 * | dbg.wline, stmtfrom, stmtto | DebugPos.line, from, to | D1 |
 * | g_dbev | Dbug.probing, Dbug.probe | D2 |
 * | op_dbg | dbug_dbg | MIRROR |
 * | dbg_depth | dbug_depth | MIRROR |
 * | dbg_pos | dbug_pos | D1 |
 * | dbg_eval | dbug_eval | MIRROR |
 * | dbg_evx, dbg_ever | dbug_done, dbug_fail | D2 |
 * | sc_tron, sc_troff | dbug_tron, dbug_troff | MIRROR |
 * | g_dbsvt, g_dbsv, g_dlprc | Saved, save, restore | MIRROR |
 *
 * - D1. A statement's place is a DebugPos that dbug_pos returns, not fields and registers.
 * - D2. The probe's return is a longjmp to a jmp_buf in Dbug; the asm unwinds its stack by hand.
 */
#include "vm/debug.h"
#include "session.h"
#include "comp/comp.h"
#include "comp/lexer.h"
#include "io/channel.h"
#include "io/console.h"
#include "mem/alloc.h"
#include "mem/program.h"
#include "mem/string.h"
#include "num/fpmath.h"
#include "vm/event.h"
#include "vm/flow.h"
#include "vm/stmts.h"

enum { PROBE_OUT_MAX = 79 };

/* Does the run stop at op, a statement's start in line? A breakpoint and until's line stop at a
   line's first statement only. */
static bool stops_at(Session *const S, const uint32_t line, const uint8_t *const op)
{
    if (S->dbug.mode == DM_STEP || (S->dbug.mode == DM_PSTEP && dbug_depth(&S->vm) <= S->dbug.pdep))
        return true;

    const Line *const ln = prog_line(&S->prog, line);

    if ((uint32_t)(addr_of(op) - S->allo.cache) != ln->code)
        return false;

    return (S->dbug.until && line == S->dbug.uline) || (S->dbug.bpon && ln->flags & LF_BP);
}

/* DBG: a statement starts. */
void dbug_dbg(Session *const S)
{
    if (!dbug_watching(&S->dbug) || S->vm.line == DLINE)
        return;

    const uint8_t *const op = S->vm.ip - 1;

    if (S->dbug.trace && S->dbug.hooks && S->dbug.hooks->trace) {
        const DebugPos at = dbug_pos(S, S->vm.line, op);
        S->dbug.hooks->trace(S, &at);
    }

    if (!stops_at(S, S->vm.line, op))
        return;

    S->dbug.mode = 0;
    S->dbug.until = false;
    stmt_stop(S);
}

uint32_t dbug_depth(const Vm *const vm)
{
    uint32_t n = 0;

    for (uint32_t i = 0; i < vm_nframes(vm); i++)
        n += vm->frames[i].kind == FRAME_CALL || vm->frames[i].kind == FRAME_GOSUB;

    return n;
}

DebugPos dbug_pos(Session *const S, const uint32_t line, const uint8_t *const code)
{
    uint32_t left;
    const Stmt *const s = flow_stmtof(&S->prog, &S->allo, line, code, &left);
    return (DebugPos){.line = line, .from = s->body, .to = left > 1 ? s[1].body : BOFF};
}

/* ---- the probe ------------------------------------------------------------------------------ */

/* What the probe's line can change of the stopped run (g_dbsvt). */
typedef struct {
    uint8_t *ip, *opip;
    Cell *sp, *rsp;
    uint32_t nframes;
    Addr fp;
    Place cont;
    uint32_t line;
    uint16_t lnum;
    uint8_t stop;
    bool in_error, running;
    jmp_buf loop;
    uint32_t tmp_used, tmp_floor;
    uint8_t capture, col;
    uint32_t sblen;
    uint8_t mode;
    bool trace, bpon, until, events_on;
    uint8_t out;
} Saved;

static void save(Session *const S, Saved *const s)
{
    *s = (Saved){
        .ip = S->vm.ip,
        .opip = S->vm.opip,
        .sp = S->vm.sp,
        .rsp = S->vm.rsp,
        .nframes = vm_nframes(&S->vm),
        .fp = S->vm.fp,
        .cont = S->vm.cont,
        .line = S->vm.line,
        .lnum = S->vm.lnum,
        .stop = S->vm.stop,
        .in_error = S->vm.err.active,
        .running = S->vm.running,
        .tmp_used = S->strg.tmp_used,
        .tmp_floor = S->strg.tmp_floor,
        .capture = S->cons.capture,
        .col = S->cons.col,
        .sblen = S->cons.sblen,
        .mode = S->dbug.mode,
        .trace = S->dbug.trace,
        .bpon = S->dbug.bpon,
        .until = S->dbug.until,
        .events_on = S->evnt.on,
        .out = S->chnl.out,
    };

    memcpy(s->loop, S->vm.loop, sizeof s->loop);
}

static void restore(Session *const S, const Saved *const s)
{
    S->vm.ip = s->ip;
    S->vm.opip = s->opip;
    S->vm.sp = s->sp;
    S->vm.rsp = s->rsp;
    S->vm.ftop = S->vm.frames + s->nframes;
    S->vm.fp = s->fp;
    S->vm.cont = s->cont;
    S->vm.line = s->line;
    S->vm.lnum = s->lnum;
    S->vm.stop = s->stop;
    S->vm.err.active = s->in_error;
    S->vm.running = s->running;
    memcpy(S->vm.loop, s->loop, sizeof S->vm.loop);
    S->strg.tmp_used = s->tmp_used;
    S->strg.tmp_floor = s->tmp_floor;
    S->cons.capture = s->capture;
    S->cons.col = s->col;
    S->cons.sblen = s->sblen;
    S->dbug.mode = s->mode;
    S->dbug.trace = s->trace;
    S->dbug.bpon = s->bpon;
    S->dbug.until = s->until;
    S->evnt.on = s->events_on;
    S->chnl.out = s->out;
}

/* Run "PRINT expr" as the direct line, its output to cons.sbuf, under its own catch: its
   error, else ST_OK. */
static Status run_probe(Session *const S, const char *const expr)
{
    static const char print[] = "PRINT ";
    memcpy(S->cons.inbuf, print, strlen(print));
    memcpy(S->cons.inbuf + strlen(print), expr, strlen(expr) + 1);
    S->vm.line = DLINE;
    Catch c;
    fault_push(&S->fault, &c);

    if (!setjmp(c.jb)) {
        S->dbug.probing = true;
        lexr_line(&S->lexr, S->cons.inbuf, S->comp.cbuf);
        comp_line(&S->comp);
        S->vm.ip = flow_dcode(S);
        S->vm.rsp = S->vm.sp;
        S->vm.running = false;
        exec_run(S);
    }

    S->dbug.probing = false;
    return c.code;
}

Status dbug_eval(Session *const S, const char *const expr, const uint32_t proc, char out[static 80])
{
    static Saved s; /* one probe at a time: no line of its can run another */
    save(S, &s);
    S->comp.dlproc = proc;
    S->dbug.mode = 0;
    S->dbug.trace = S->dbug.bpon = S->dbug.until = false;
    S->vm.stop = 0;
    S->evnt.on = false;
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    const Status st = run_probe(S, expr);
    const uint32_t n = S->cons.sblen < PROBE_OUT_MAX ? S->cons.sblen : PROBE_OUT_MAX;
    memcpy(out, S->cons.sbuf, n);
    out[n] = 0;
    restore(S, &s);
    S->comp.dlproc = 0;
    fmth_clear();
    return st;
}

void dbug_done(Session *const S) { fault_throw(&S->fault, ST_OK); }

/* TRON, TROFF: trace on, off. */
void dbug_tron(Session *const S) { S->dbug.trace = true; }
void dbug_troff(Session *const S) { S->dbug.trace = false; }
