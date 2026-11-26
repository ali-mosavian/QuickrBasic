/* The debugger's view of a session: its hooks, stepping, and a stopped run's state.
 *
 * # How it works
 * The functions set the debug hooks and stepping state, and read a stopped run's frames,
 * variables and next line from the VM's call frames.
 */
#include "session.h"
#include "io/errmsg.h"
#include "vm/flow.h"
#include "vm/stmts.h"

void session_debug(Session *const S, const DebugHooks *const hooks)
{
    S->dbug.emit = true;
    S->dbug.hooks = hooks;
}

void session_step(Session *const S, const StepKind kind, const bool stopped)
{
    const uint32_t depth = stopped ? dbug_depth(&S->vm) : 0;

    switch (kind) {
    case STEP_INTO:
        S->dbug.mode = DM_STEP;
        return;

    case STEP_OVER:
        S->dbug.pdep = depth;
        break;

    case STEP_OUT:
        S->dbug.pdep = depth ? depth - 1 : 0;
        break;
    }

    S->dbug.mode = DM_PSTEP;
}

void session_arm(Session *const S)
{
    S->dbug.bpon = true;
    S->dbug.until = false;
}

void session_until(Session *const S, const uint32_t line)
{
    S->dbug.uline = line;
    S->dbug.until = true;
}

void session_trace(Session *const S, const bool on) { S->dbug.trace = on; }

void session_halting(Session *const S, const bool on) { S->dbug.halting = on; }

bool session_tracing(const Session *const S) { return S->dbug.trace; }

bool session_stopped_at(Session *const S, DebugPos *const pos)
{
    if (!S->vm.cont.line)
        return false;

    *pos = dbug_pos(S, S->vm.cont.line, at(S->allo.cache + S->vm.cont.code));
    return true;
}

void session_drop_stop(Session *const S)
{
    flow_drop_calls(S);
    S->vm.cont.line = 0;
    natv_stop(&S->natv);
}

bool session_set_next(Session *const S, const uint32_t line)
{
    comp_proc(&S->comp, blok_procof(&S->blok, line));
    const uint32_t proc = S->comp.proc;
    comp_proc(&S->comp, blok_procof(&S->blok, S->vm.cont.line));

    if (proc != S->comp.proc)
        return false;

    S->vm.cont.line = line;
    S->vm.cont.code = prog_line(&S->prog, line)->code;
    return true;
}

/* The newest call frame at index i or older, -1 none. */
static int32_t call_from(const Vm *const vm, int32_t i)
{
    for (; i >= 0; i--)
        if (frame_is_call(&vm->frames[i]))
            return i;

    return -1;
}

/* A walk down the stopped run's frames: the VM frame to look for the next call from, the line the
   next frame is at; done past the module's. */
typedef struct {
    int32_t i;
    uint32_t line;
    bool done;
} FrameWalk;

static FrameWalk walk_start(const Session *const S)
{
    return (FrameWalk){.i = (int32_t)vm_nframes(&S->vm) - 1, .line = S->vm.cont.line};
}

/* The walk's next frame to *out; false none. */
static bool walk_next(const Session *const S, FrameWalk *const w, DebugFrame *const out)
{
    if (w->done)
        return false;

    const int32_t c = call_from(&S->vm, w->i);
    *out = (DebugFrame){.line = w->line, .call = c, .proc = c < 0 ? 0 : S->vm.frames[c].call.proc};

    w->done = c < 0;

    if (c >= 0) {
        w->line = S->vm.frames[c].ret.line;
        w->i = c - 1;
    }

    return true;
}

uint32_t session_frames(const Session *const S, DebugFrame out[], const uint32_t max)
{
    FrameWalk w = walk_start(S);
    uint32_t n = 0;

    while (n < max && walk_next(S, &w, &out[n]))
        n++;

    return n;
}

bool session_frame(const Session *const S, const uint32_t i, DebugFrame *const out)
{
    FrameWalk w = walk_start(S);

    for (uint32_t n = 0; walk_next(S, &w, out); n++)
        if (n == i)
            return true;

    return false;
}

uint32_t session_nframes(const Session *const S)
{
    FrameWalk w = walk_start(S);
    DebugFrame f;
    uint32_t n = 0;

    while (walk_next(S, &w, &f))
        n++;

    return n;
}

Addr session_cell(
    const Session *const S,
    const DebugFrame *const f,
    const uint32_t id,
    const int32_t par
)
{
    return symb_at(&S->symb, f->call < 0 ? 0 : S->vm.frames[f->call].call.fp, id);
}

bool session_in_direct(const Session *const S)
{
    for (uint32_t i = 0; i < vm_nframes(&S->vm); i++)
        if (S->vm.frames[i].ret.line == DLINE)
            return true;

    return false;
}

/* The stopped run's newest frame, its line 0 none. */
static DebugFrame session_top(const Session *const S)
{
    DebugFrame f = {};

    session_frame(S, 0, &f);
    return f;
}

/* The stopped run's newest frame in procedure proc's scope, at a program line, to *f; false
   none. */
static bool scope_frame(const Session *const S, const uint32_t proc, DebugFrame *const f)
{
    FrameWalk w = walk_start(S);

    while (walk_next(S, &w, f))
        if (f->proc == proc && f->line && f->line != DLINE)
            return true;

    return false;
}

/* The dialect of the stopped run's code in procedure proc's scope: its frame's line's; none, the
   program's. */
static const Dialect *scope_dialect(const Session *const S, const uint32_t proc)
{
    DebugFrame f;
    return scope_frame(S, proc, &f) ? dial_line(&S->prog, f.line) : dial_program(&S->prog);
}

/* A QBasic probe of text in procedure proc's scope, its names its frame's module's (dbug_eval,
   dbug_exec). */
static Status qbasic_probe(
    Session *const S,
    Status (*const probe)(Session *S, const char *text, uint32_t proc, char out[static 80]),
    const char *const text,
    const uint32_t proc,
    char out[static 80]
)
{
    const uint32_t was = S->repl.module;
    DebugFrame f;

    if (scope_frame(S, proc, &f))
        S->repl.module = modu_of(&S->modu, prog_line(&S->prog, f.line)->file);

    const Status st = probe(S, text, proc, out);
    S->repl.module = was;
    return st;
}

/* A probe's line, its scope's dialect's, as the direct line's code, at the stopped statement
   when it is in the probe's procedure, else after the modules' last line. */
static void session_text_code(Session *const S)
{
    const DebugFrame f = session_top(S);
    scope_dialect(S, S->comp.dlproc)->direct(S, f.proc == S->comp.dlproc ? f.line : 0);
}

void session_text_line(Session *const S)
{
    const DebugFrame f = session_top(S);
    scope_dialect(S, f.proc)->direct(S, f.line);
}

const Dialect *session_dialect(const Session *const S)
{
    return scope_dialect(S, session_top(S).proc);
}

Status session_eval(
    Session *const S,
    const char *const expr,
    const uint32_t proc,
    char out[static 80]
)
{
    const Dialect *const d = scope_dialect(S, proc);

    if (!d->text)
        return qbasic_probe(S, dbug_eval, expr, proc, out);

    enum { TEXT = 256 };

    char text[TEXT];

    if (!d->print(expr, text, TEXT))
        return E_LS;

    return dbug_probe(S, session_text_code, text, proc, out);
}

Status session_exec(
    Session *const S,
    const char *const stmt,
    const uint32_t proc,
    char out[static 80]
)
{
    return scope_dialect(S, proc)->text ? dbug_probe(S, session_text_code, stmt, proc, out)
                                        : qbasic_probe(S, dbug_exec, stmt, proc, out);
}

const char *session_errtext(const Session *const S, const uint8_t code)
{
    const char *const said = dial_errtext(S, code);
    return said ? said : emsg_text(S->vm.err.user, code);
}

void session_clear(Session *const S)
{
    symb_clear(&S->symb);
    allo_shrink(&S->allo);
    arry_clear_heap(&S->arry);
    S->strg.tmp_used = 0;
    S->strg.tmp_floor = 0;
    stmt_restore(S);
    vdev_clear(&S->vdev);
}

void session_drop(Session *const S)
{
    natv_stop(&S->natv); /* a stop's run ends */
    S->vm.cont.line = 0;
    S->link.cursor = 0;
    S->link.linked = false;
    S->link.start = 0;
    S->link.lmask = 0;
    flow_drop_calls(S);
    prog_terminate(&S->prog);
    session_clear(S);
}

void session_new(Session *const S)
{
    dial_forget(S);
    S->prog.end = PROGBASE;
    memset(S->lexr.off, 0, sizeof S->lexr.off); /* no OPTION DISABLE yet */
    symb_reset(&S->symb);
    decl_reset(&S->decl, &S->scop, &S->symb);
    session_drop(S);
}

static void session_edited(ProgUser *const u)
{
    Session *const S = container_of(u, Session, prog_user);
    dial_forget(S);
    session_drop(S);
}
static void session_growing(ProgUser *const u)
{
    session_clear(container_of(u, Session, prog_user));
}

ProgUser *session_prog_user(Session *const S)
{
    S->prog_user = (ProgUser){.edited = session_edited, .growing = session_growing};

    return &S->prog_user;
}

static void session_run_until(CompRun *const r, const uint8_t *const end)
{
    exec_until(container_of(r, Session, comp_run), end);
}

CompRun *session_comp_run(Session *const S)
{
    S->comp_run = (CompRun){.until = session_run_until};

    return &S->comp_run;
}
