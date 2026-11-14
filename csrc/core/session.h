/* A BASIC session: every module's state, one owner. Each function that reads or changes any of
   it takes the Session as S, its first parameter. */
#ifndef SESSION_H
#define SESSION_H
#include "comp/blok.h"
#include "comp/comp.h"
#include "comp/ctrl.h"
#include "comp/decl.h"
#include "comp/expr.h"
#include "comp/lexr.h"
#include "comp/link.h"
#include "comp/list.h"
#include "io/chnl.h"
#include "io/cons.h"
#include "io/diag.h"
#include "io/file.h"
#include "io/repl.h"
#include "io/srcr.h"
#include "mem/allo.h"
#include "mem/arry.h"
#include "mem/prog.h"
#include "mem/scop.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "num/rndm.h"
#include "vm/chai.h"
#include "vm/dbug.h"
#include "vm/evnt.h"
#include "vm/exec.h"
#include "vm/flow.h"
#include "vm/stmt.h"
#include "vm/syst.h"
#include "vm/usng.h"

struct Session {
    Allo allo;
    Arry arry;
    Blok blok;
    Chai chai;
    Chnl chnl;
    Comp comp;
    Cons cons;
    Ctrl ctrl;
    Dbug dbug;
    Decl decl;
    Diag diag;
    Evnt evnt;
    Expr expr;
    File file;
    Flow flow;
    Lexr lexr;
    Link link;
    List list;
    Prog prog;
    Repl repl;
    Rndm rndm;
    Scop scop;
    Srcr srcr;
    StmtState stmt;
    Strg strg;
    Symb symb;
    Syst syst;
    Usng usng;
    Vm vm;
};

/* The state a session starts with before boot: what is not zero. */
void session_reset(Session *S);

/* ---- the debugger --------------------------------------------------------------------------- */

typedef enum : uint8_t { STEP_INTO, STEP_OVER, STEP_OUT } StepKind;

/* Every statement starts with a DBG op from now on; the VM reports to hooks. */
void session_debug(Session *S, const DebugHooks *hooks);
/* The next run or CONT stops after a step of kind; stopped: from the stop, else from the start. */
void session_step(Session *S, StepKind kind, bool stopped);
/* Before a run or CONT: breakpoints stop it, no until line. */
void session_arm(Session *S);
/* The run also stops once at program line line's first statement. */
void session_until(Session *S, uint32_t line);
void session_trace(Session *S, bool on);
bool session_tracing(const Session *S);
/* Where the stopped run goes on: false, none. */
bool session_stopped_at(Session *S, DebugPos *pos);
/* The stopped run dropped: CONT cannot go on. */
void session_drop_stop(Session *S);
/* CONT goes on at program line line; false: not the stopped procedure's line. */
bool session_set_next(Session *S, uint32_t line);
#endif
