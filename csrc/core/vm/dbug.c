/* The debugger's VM side (src/vm/dbg.asm): where a run stops, TRON, TROFF, the watch probe. */
#include "vm/dbug.h"
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

Dbug dbug;


static bool watching(void) { return dbug.mode || dbug.trace || dbug.bpon || dbug.until; }

/* Does the run stop at op, a statement's start in line? A breakpoint and until's line stop at a
   line's first statement only. */
static bool stops_at(const uint32_t line, const uint8_t *const op)
{
    if (dbug.mode == DM_STEP || (dbug.mode == DM_PSTEP && dbug_depth(&vm) <= dbug.pdep))
        return true;
    const Line *const ln = prog_line(line);
    if ((uint32_t)(addr_of(op) - allo.cache) != ln->code)
        return false;
    return (dbug.until && line == dbug.uline) || (dbug.bpon && ln->flags & LF_BP);
}

/* DBG: a statement starts. */
void dbug_dbg(Vm *const vm)
{
    if (!watching() || vm->line == DLINE)
        return;
    const uint8_t *const op = vm->ip - 1;
    if (dbug.trace && dbug.hook) {
        dbug_pos(vm->line, op);
        dbug.hook();
    }
    if (!stops_at(vm->line, op))
        return;
    dbug.mode = 0;
    dbug.until = false;
    stmt_stop(vm);
}

uint32_t dbug_depth(const Vm *const vm)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < vm->nframes; i++)
        n += vm->frames[i].kind == FRAME_CALL || vm->frames[i].kind == FRAME_GOSUB;
    return n;
}

void dbug_pos(const uint32_t line, const uint8_t *const code)
{
    dbug.wline = line;
    uint32_t left;
    const Stmt *const s = flow_stmtof(line, code, &left);
    dbug.stmt_from = s->body;
    dbug.stmt_to = left > 1 ? s[1].body : BOFF;
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

static void save(Saved *const s)
{
    *s = (Saved){
        .ip = vm.ip, .opip = vm.opip, .sp = vm.sp, .rsp = vm.rsp,
        .nframes = vm.nframes, .ncallvars = vm.ncallvars, .cont = vm.cont,
        .line = vm.line, .lnum = vm.lnum, .stop = vm.stop,
        .in_error = vm.err.active, .running = vm.running,
        .tmp_used = strg.tmp_used, .tmp_floor = strg.tmp_floor,
        .capture = cons.capture, .col = cons.col, .sblen = cons.sblen,
        .mode = dbug.mode, .trace = dbug.trace, .bpon = dbug.bpon, .until = dbug.until,
        .events_on = evnt.on, .out = chnl.out,
    };
    memcpy(s->loop, vm.loop, sizeof s->loop);
}

static void restore(const Saved *const s)
{
    vm.ip = s->ip;
    vm.opip = s->opip;
    vm.sp = s->sp;
    vm.rsp = s->rsp;
    vm.nframes = s->nframes;
    vm.ncallvars = s->ncallvars;
    vm.cont = s->cont;
    vm.line = s->line;
    vm.lnum = s->lnum;
    vm.stop = s->stop;
    vm.err.active = s->in_error;
    vm.running = s->running;
    memcpy(vm.loop, s->loop, sizeof vm.loop);
    strg.tmp_used = s->tmp_used;
    strg.tmp_floor = s->tmp_floor;
    cons.capture = s->capture;
    cons.col = s->col;
    cons.sblen = s->sblen;
    dbug.mode = s->mode;
    dbug.trace = s->trace;
    dbug.bpon = s->bpon;
    dbug.until = s->until;
    evnt.on = s->events_on;
    chnl.out = s->out;
}

/* Run "PRINT expr" as the direct line, its output to cons.sbuf; how it came back. */
static int run_probe(const char *const expr)
{
    static const char print[] = "PRINT ";
    memcpy(cons.inbuf, print, strlen(print));
    memcpy(cons.inbuf + strlen(print), expr, strlen(expr) + 1);
    vm.line = DLINE;
    const int how = setjmp(dbug.probe);
    if (how == PROBE_RUNNING) {
        dbug.probing = true;
        lexr_line(cons.inbuf, comp.cbuf);
        comp_line();
        vm.ip = flow_dcode();
        vm.rsp = vm.sp;
        vm.running = false;
        exec_run();
    }
    dbug.probing = false;
    return how;
}

uint8_t dbug_eval(const char *const expr, const uint8_t proc, char out[static 80])
{
    static Saved s; /* one probe at a time: no line of its can run another */
    save(&s);
    comp.dlproc = proc;
    dbug.mode = 0;
    dbug.trace = dbug.bpon = dbug.until = false;
    vm.stop = 0;
    evnt.on = false;
    cons.capture = CAP_SBUF;
    cons.sblen = 0;
    const int how = run_probe(expr);
    const uint32_t n = cons.sblen < PROBE_OUT_MAX ? cons.sblen : PROBE_OUT_MAX;
    memcpy(out, cons.sbuf, n);
    out[n] = 0;
    restore(&s);
    comp.busy = false;
    comp.dlproc = 0;
    fmth_clear();
    return (uint8_t)(how - PROBE_DONE);
}

void dbug_done(void) { longjmp(dbug.probe, PROBE_DONE); }

void dbug_fail(const uint8_t code) { longjmp(dbug.probe, PROBE_FAILED + code); }

/* TRON, TROFF: trace on, off. */
void dbug_tron(Vm *const) { dbug.trace = true; }
void dbug_troff(Vm *const) { dbug.trace = false; }
