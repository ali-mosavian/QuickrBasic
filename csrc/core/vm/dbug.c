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

Dbug dbug;

static jmp_buf probe; /* g_dbev: dbug_eval's, while its line runs */

/* DBG: a statement starts. */
void dbug_dbg(void)
{
    if (!(dbug.mode | dbug.trace | dbug.bpon | dbug.until))
        return;
    uint32_t line = exec.line;
    if (line == DLINE)
        return;
    const uint8_t *op = exec.ip - 1;
    if (dbug.trace && dbug.hook) {
        dbug_pos(line, op);
        dbug.hook();
        line = exec.line;
    }
    bool stop = dbug.mode == DM_STEP || (dbug.mode == DM_PSTEP && dbug_depth() <= dbug.pdep);
    if (!stop && (uint32_t)(addr_of(op) - allo.cache) == prog_line(line)->code)
        stop = (dbug.until && line == dbug.uline) || /* the line's first statement only */
               (dbug.bpon && prog_line(line)->flags & LF_BP);
    if (!stop)
        return;
    dbug.mode = 0;
    dbug.until = false;
    stmt_stop();
}

uint32_t dbug_depth(void)
{
    uint32_t n = 0;
    for (Addr f = exec.cstk; f < allo.cs_top; f += rd8(f))
        n += rd8(f) == M_PC || rd8(f) == M_GS;
    return n;
}

void dbug_pos(uint32_t line, const uint8_t *code)
{
    dbug.wline = line;
    uint32_t left;
    const Stmt *e = flow_stmtof(line, code, &left);
    dbug.stmt_from = e->body;
    dbug.stmt_to = left > 1 ? e[1].body : BOFF;
}

/* What the probe's line can change of the stopped run (g_dbsvt). */
typedef struct {
    uint8_t *ip, *opip;
    Cell *sp, *rsp;
    Addr cstk, lsp;
    uint32_t cont_line, cont_code, line, runln;
    uint16_t lnum;
    uint8_t stop;
    bool inerr, running;
    uint16_t tmp_used, tmp_floor;
    uint8_t capture, col;
    uint32_t sblen;
    uint8_t mode;
    bool trace, bpon, until, evon;
    uint8_t out;
} Saved;

static jmp_buf loop; /* exec.loop of the stopped run */

static void save(Saved *s)
{
    *s = (Saved){exec.ip, exec.opip, exec.sp, exec.rsp, exec.cstk, exec.lsp, exec.cont_line,
                 exec.cont_code, exec.line, exec.runln, exec.lnum, exec.stop, exec.inerr,
                 exec.running, strg.tmp_used, strg.tmp_floor, cons.capture, cons.col,
                 cons.sblen, dbug.mode, dbug.trace, dbug.bpon, dbug.until, evnt.on,
                 chnl.out};
    memcpy(loop, exec.loop, sizeof loop);
}

static void restore(const Saved *s)
{
    exec.ip = s->ip;
    exec.opip = s->opip;
    exec.sp = s->sp;
    exec.rsp = s->rsp;
    exec.cstk = s->cstk;
    exec.lsp = s->lsp;
    exec.cont_line = s->cont_line;
    exec.cont_code = s->cont_code;
    exec.line = s->line;
    exec.runln = s->runln;
    exec.lnum = s->lnum;
    exec.stop = s->stop;
    exec.inerr = s->inerr;
    exec.running = s->running;
    strg.tmp_used = s->tmp_used;
    strg.tmp_floor = s->tmp_floor;
    cons.capture = s->capture;
    cons.col = s->col;
    cons.sblen = s->sblen;
    dbug.mode = s->mode;
    dbug.trace = s->trace;
    dbug.bpon = s->bpon;
    dbug.until = s->until;
    evnt.on = s->evon;
    chnl.out = s->out;
    memcpy(exec.loop, loop, sizeof exec.loop);
}

uint8_t dbug_eval(const char *expr, uint8_t proc, char out[80])
{
    static Saved s; /* one probe at a time: no line of its can run another */
    save(&s);
    comp.dlproc = proc;
    dbug.mode = 0;
    dbug.trace = dbug.bpon = dbug.until = false;
    exec.stop = 0;
    evnt.on = false;
    cons.capture = CAP_SBUF;
    cons.sblen = 0;
    uint8_t *t = cons.inbuf; /* "PRINT " and the expression */
    memcpy(t, "PRINT ", 6);
    memcpy(t + 6, expr, strlen(expr) + 1);
    exec.line = DLINE;
    int r = setjmp(probe);
    if (!r) {
        dbug.probing = true;
        lexr_line(cons.inbuf, comp_cbuf);
        comp_line();
        exec.ip = flow_dcode();
        exec.rsp = exec.sp;
        exec.running = false;
        exec_run();
    }
    dbug.probing = false;
    uint32_t n = cons.sblen < 79 ? cons.sblen : 79;
    memcpy(out, cons.sbuf, n);
    out[n] = 0;
    restore(&s);
    comp.busy = false;
    comp.dlproc = 0;
    fmth_clear();
    return (uint8_t)(r - 1);
}

void dbug_done(void) { longjmp(probe, 1); }

void dbug_fail(uint8_t code) { longjmp(probe, code + 2); }

/* TRON, TROFF: trace on, off. */
void dbug_tron(void) { dbug.trace = true; }
void dbug_troff(void) { dbug.trace = false; }
