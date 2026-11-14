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
#endif
