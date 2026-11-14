/* The debugger's view of a session: its hooks, stepping, and a stopped run's state.
 *
 * # How it works
 * The functions set the debug hooks and stepping state, and read a stopped run's frames,
 * variables and next line from the VM's call frames.
 */
#include "session.h"
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
    S->vm.nframes = 0;
    S->vm.cont.line = 0;
}

bool session_set_next(Session *const S, const uint32_t line)
{
    comp_proc(&S->comp, blok_procof(&S->blok, line));
    const uint8_t proc = S->comp.proc;
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
        if (vm->frames[i].kind == FRAME_CALL)
            return i;

    return -1;
}

uint32_t session_frames(const Session *const S, DebugFrame out[], const uint32_t max)
{
    uint32_t n = 0;
    uint32_t line = S->vm.cont.line;

    for (int32_t i = (int32_t)S->vm.nframes - 1; n < max;) {
        const int32_t c = call_from(&S->vm, i);
        out[n++] =
            (DebugFrame){.line = line, .call = c, .proc = c < 0 ? 0 : S->vm.frames[c].call.proc};

        if (c < 0)
            break;

        line = S->vm.frames[c].ret.line;
        i = c - 1;
    }

    return n;
}

/* The oldest call of f's procedure newer than f, which saved what f sees; nullptr none. */
static const Call *newer_call(const Vm *const vm, const DebugFrame *const f)
{
    const Call *newer = nullptr;

    if (f->call < 0)
        return nullptr;
    for (uint32_t i = vm->nframes - 1; i > (uint32_t)f->call; i--)
        if (vm->frames[i].kind == FRAME_CALL && vm->frames[i].call.proc == f->proc)
            newer = &vm->frames[i].call;

    return newer;
}

Addr session_cell(
    const Session *const S,
    const DebugFrame *const f,
    const uint16_t id,
    const int32_t par
)
{
    const Addr vt = symb_var(&S->symb, id)->vt;
    const Call *const c = newer_call(&S->vm, f);

    if (!c)
        return vt;

    /* a parameter: its cell before the newer call */
    const Addr cell = par >= 0 ? S->vm.callvars[c->params + (uint32_t)par].cell : vt;

    if (cell != symb_cell(&S->symb, id))
        return cell;

    uint32_t n = 0; /* its own: the value saved */
    for (uint16_t v = S->symb.first_local[f->proc]; v; v = symb_varx(&S->symb, v)->nloc, n++)
        if (v == id)
            return addr_of(&S->vm.callvars[c->locals + n].value);

    return cell;
}

bool session_in_direct(const Session *const S)
{
    for (uint32_t i = 0; i < S->vm.nframes; i++)
        if (S->vm.frames[i].ret.line == DLINE)
            return true;

    return false;
}

uint8_t session_eval(
    Session *const S,
    const char *const expr,
    const uint8_t proc,
    char out[static 80]
)
{
    return dbug_eval(S, expr, proc, out);
}

void session_clear(Session *const S)
{
    symb_clear(&S->symb);
    S->allo.arrays = S->allo.bot;
    S->allo.strings = S->allo.top;
    S->arry.hdr = S->allo.hdr_base;
    S->strg.tmp_used = 0;
    S->strg.tmp_floor = 0;
    S->vm.ncallvars = 0;
    stmt_restore(S);
}

void session_drop(Session *const S)
{
    S->vm.cont.line = 0;
    S->link.cursor = 0;
    S->link.linked = false;
    S->vm.nframes = 0;
    prog_terminate(&S->prog);
    session_clear(S);
}

void session_new(Session *const S)
{
    S->prog.end = PROGBASE;
    symb_reset(&S->symb);
    decl_reset(&S->decl, &S->scop, &S->symb);
    session_drop(S);
}

static void session_edited(ProgUser *const u) { session_drop(container_of(u, Session, prog_user)); }
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
