/* The debugger's VM side (src/vm/dbg.asm): where a run stops, TRON, TROFF, the watch probe. */
#include "vm/dbug.h"
#include "session.h"
#include "comp/comp.h"
#include "comp/lexr.h"
#include "io/chnl.h"
#include "io/cons.h"
#include "mem/allo.h"
#include "mem/prog.h"
#include "mem/strg.h"
#include "num/fmth.h"
#include "vm/evnt.h"
#include "vm/flow.h"
#include "vm/stmt.h"

enum { PROBE_OUT_MAX = 79 };

/* How the probe's line came back to dbug_eval: setjmp's value. */
enum { PROBE_RUNNING = 0, PROBE_DONE = 1, PROBE_FAILED = 2 /* + the error */ };



static bool watching(Session *const S) { return S->dbug.mode || S->dbug.trace || S->dbug.bpon || S->dbug.until; }

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
    if (!watching(S) || S->vm.line == DLINE)
        return;
    const uint8_t *const op = S->vm.ip - 1;
    if (S->dbug.trace && S->dbug.hook) {
        dbug_pos(S, S->vm.line, op);
        S->dbug.hook(S);
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
    for (uint32_t i = 0; i < vm->nframes; i++)
        n += vm->frames[i].kind == FRAME_CALL || vm->frames[i].kind == FRAME_GOSUB;
    return n;
}

void dbug_pos(Session *const S, const uint32_t line, const uint8_t *const code)
{
    S->dbug.wline = line;
    uint32_t left;
    const Stmt *const s = flow_stmtof(S, line, code, &left);
    S->dbug.stmt_from = s->body;
    S->dbug.stmt_to = left > 1 ? s[1].body : BOFF;
}

/* ---- the probe ------------------------------------------------------------------------------ */

/* What the probe's line can change of the stopped run (g_dbsvt). */
typedef struct {
    uint8_t *ip, *opip;
    Cell *sp, *rsp;
    uint32_t nframes, ncallvars;
    Place cont;
    uint32_t line;
    uint16_t lnum;
    uint8_t stop;
    bool in_error, running;
    jmp_buf loop;
    uint16_t tmp_used, tmp_floor;
    uint8_t capture, col;
    uint32_t sblen;
    uint8_t mode;
    bool trace, bpon, until, events_on;
    uint8_t out;
} Saved;

static void save(Session *const S, Saved *const s)
{
    *s = (Saved){
        .ip = S->vm.ip, .opip = S->vm.opip, .sp = S->vm.sp, .rsp = S->vm.rsp,
        .nframes = S->vm.nframes, .ncallvars = S->vm.ncallvars, .cont = S->vm.cont,
        .line = S->vm.line, .lnum = S->vm.lnum, .stop = S->vm.stop,
        .in_error = S->vm.err.active, .running = S->vm.running,
        .tmp_used = S->strg.tmp_used, .tmp_floor = S->strg.tmp_floor,
        .capture = S->cons.capture, .col = S->cons.col, .sblen = S->cons.sblen,
        .mode = S->dbug.mode, .trace = S->dbug.trace, .bpon = S->dbug.bpon, .until = S->dbug.until,
        .events_on = S->evnt.on, .out = S->chnl.out,
    };
    memcpy(s->loop, S->vm.loop, sizeof s->loop);
}

static void restore(Session *const S, const Saved *const s)
{
    S->vm.ip = s->ip;
    S->vm.opip = s->opip;
    S->vm.sp = s->sp;
    S->vm.rsp = s->rsp;
    S->vm.nframes = s->nframes;
    S->vm.ncallvars = s->ncallvars;
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

/* Run "PRINT expr" as the direct line, its output to cons.sbuf; how it came back. */
static int run_probe(Session *const S, const char *const expr)
{
    static const char print[] = "PRINT ";
    memcpy(S->cons.inbuf, print, strlen(print));
    memcpy(S->cons.inbuf + strlen(print), expr, strlen(expr) + 1);
    S->vm.line = DLINE;
    const int how = setjmp(S->dbug.probe);
    if (how == PROBE_RUNNING) {
        S->dbug.probing = true;
        lexr_line(S, S->cons.inbuf, S->comp.cbuf);
        comp_line(S);
        S->vm.ip = flow_dcode(S);
        S->vm.rsp = S->vm.sp;
        S->vm.running = false;
        exec_run(S);
    }
    S->dbug.probing = false;
    return how;
}

uint8_t dbug_eval(Session *const S, const char *const expr, const uint8_t proc, char out[static 80])
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
    const int how = run_probe(S, expr);
    const uint32_t n = S->cons.sblen < PROBE_OUT_MAX ? S->cons.sblen : PROBE_OUT_MAX;
    memcpy(out, S->cons.sbuf, n);
    out[n] = 0;
    restore(S, &s);
    S->comp.busy = false;
    S->comp.dlproc = 0;
    fmth_clear();
    return (uint8_t)(how - PROBE_DONE);
}

void dbug_done(Session *const S) { longjmp(S->dbug.probe, PROBE_DONE); }

void dbug_fail(Session *const S, const uint8_t code) { longjmp(S->dbug.probe, PROBE_FAILED + code); }

/* TRON, TROFF: trace on, off. */
void dbug_tron(Session *const S) { S->dbug.trace = true; }
void dbug_troff(Session *const S) { S->dbug.trace = false; }
