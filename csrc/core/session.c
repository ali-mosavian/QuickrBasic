#include "session.h"

void session_reset(Session *const S)
{
    S->scop.nbnd = 1;
    S->scop.depth = 1;
}

void session_debug(Session *const S, const DebugHooks *const hooks)
{
    S->dbug.emit = true;
    S->dbug.hooks = hooks;
}

void session_step(Session *const S, const StepKind kind, const bool stopped)
{
    const uint32_t depth = stopped ? dbug_depth(&S->vm) : 0;
    switch (kind) {
    case STEP_INTO: S->dbug.mode = DM_STEP; return;
    case STEP_OVER: S->dbug.pdep = depth; break;
    case STEP_OUT: S->dbug.pdep = depth ? depth - 1 : 0; break;
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
    blok_scope(S, line);
    const uint8_t proc = S->comp.proc;
    blok_scope(S, S->vm.cont.line);
    if (proc != S->comp.proc)
        return false;
    const uint32_t code = prog_line(&S->prog, line)->code;
    const Addr a = code ? S->allo.cache + code : link_cache(S, line);
    S->vm.cont.line = line;
    S->vm.cont.code = a - S->allo.cache;
    return true;
}
