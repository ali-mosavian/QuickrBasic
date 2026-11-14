/* A BASIC session: every module's state, one owner.
 *
 * # How it works
 * Each function that reads or changes any of the state takes the Session as S, its first
 * parameter. The debugger's functions (session_*) are implemented in session.c.
 */
#ifndef SESSION_H
#define SESSION_H
#include "fault.h"
#include "comp/ast.h"
#include "comp/blocks.h"
#include "comp/comp.h"
#include "comp/control.h"
#include "comp/declare.h"
#include "comp/expr.h"
#include "comp/codegen.h"
#include "comp/lexer.h"
#include "comp/link.h"
#include "comp/listing.h"
#include "io/channel.h"
#include "io/console.h"
#include "io/diag.h"
#include "io/file.h"
#include "io/repl.h"
#include "io/source.h"
#include "mem/alloc.h"
#include "mem/array.h"
#include "mem/program.h"
#include "mem/scope.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "num/random.h"
#include "vm/chain.h"
#include "vm/debug.h"
#include "vm/event.h"
#include "vm/exec.h"
#include "vm/flow.h"
#include "vm/stmts.h"
#include "vm/system.h"
#include "vm/using.h"

struct Session {
    Allo allo;
    Ast ast;
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
    Code code;
    Fault fault;
    LexrNames names;
    UsngOut usng_out;
    File file;
    Flow flow;
    Lexr lexr;
    Link link;
    List list;
    Prog prog;
    ProgUser prog_user;
    CompRun comp_run;
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

/* ---- the debugger --------------------------------------------------------------------------- */

typedef enum : uint8_t { STEP_INTO, STEP_OVER, STEP_OUT } StepKind;

/* Every statement starts with a DBG op from now on; the VM reports to hooks. */
/* CLR: variables zeroed, arrays dropped, READ reset, the heap and the temporaries emptied. */
void session_clear(Session *S);
/* After an edit: CONT and the link's cursor end, the frames drop, the store's terminator, then
   session_clear. The next link empties the cache. */
void session_drop(Session *S);
/* NEW: no program, no names, no variables, no declarations, then session_drop. */
void session_new(Session *S);
/* The store's user: session_drop when edited, session_clear before it grows. */
ProgUser *session_prog_user(Session *S);
/* The session as the compiler's runner. */
CompRun *session_comp_run(Session *S);
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

/* A frame of the stopped run: the program line it is at, its procedure (0 the module) and that
   procedure's call frame (-1 the module). */
typedef struct {
    uint32_t line;
    uint8_t proc;
    int32_t call;
} DebugFrame;

/* The stopped run's frames to out, newest first, at most max; how many. */
uint32_t session_frames(const Session *S, DebugFrame out[], uint32_t max);
/* Variable id's cell as frame f sees it; par its parameter's place, -1 none. */
Addr session_cell(const Session *S, const DebugFrame *f, uint16_t id, int32_t par);
/* A direct line called into the stopped run: evaluating would drop its code. */
bool session_in_direct(const Session *S);
/* PRINT expr in procedure proc's scope to out (dbug_eval): its error, else ST_OK. */
Status session_eval(Session *S, const char *expr, uint8_t proc, char out[static 80]);
#endif
