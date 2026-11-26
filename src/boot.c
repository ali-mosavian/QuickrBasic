/* The start: the arena, its fixed regions, the blocks, the parts' state.
 *
 * # How it works
 * boot sets the error handler, takes the arena from the platform (exiting with "Out of memory" when
 * it gives too little, or when the regions do not fit it: an error then has no REPL to land in),
 * then runs each module's init in the asm's order (main.asm's rt_start):
 * fixed regions first, then the blocks (code cache, program store), the front end's regions, and
 * the symbol tables last, as they grow while code holds their cells. The front end's regions are
 * placed with the program's.
 */
#include "boot.h"
#include "session.h"
#include "comp/comp.h"
#include "comp/lexer.h"
#include "io/source.h"
#include "io/console.h"
#include "io/file.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/loc.h"
#include "mem/program.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "num/fpmath.h"
#include "num/random.h"
#include "plat/pfil.h"
#include "plat/pmem.h"
#include "plat/psys.h"
#include "qbrt/qbrt.h"
#include "vm/exec.h"
#include "vm/flow.h"
#include "vm/stmts.h"
#include "vm/system.h"

[[noreturn]]
static void out_of_memory(void)
{
    static const char msg[] = "Out of memory\r\n";
    uint32_t put;
    pfil_write(PFIL_STDERR, msg, sizeof msg - 1, &put);
    psys_exit(1);
}

void boot(Session *const S, const FrontEnd *const fe)
{
    S->repl.fe = fe;
    S->fault.raise = repl_fault;
    decl_init(&S->decl, &S->fault);
    S->names.intern = repl_intern;
    S->impt.find = repl_runs;
    lexr_init(&S->lexr, &S->fault, &S->names, &S->comp.cbuf, &S->comp.ccap);
    stmt_usng_init(S);

    if (!pmem_init())
        out_of_memory();

    Catch c;
    fault_push(&S->fault, &c);

    if (setjmp(c.jb))
        out_of_memory();

    scop_init(&S->scop, &S->fault); /* its arena's first block from the C side's heap */

    allo_init(&S->allo, &S->fault); /* the arrays' headers, the control stack, the POKE memory */
    qbrt_init(&S->fault, &S->vm.stop, at(allo_carve(&S->allo, QBRT_HEAP)), QBRT_HEAP);
    strg_init(&S->strg, &S->allo, &S->fault, &S->prog);
    arry_init(&S->arry, &S->allo, &S->fault, &S->symb, &S->decl, &S->strg, &S->vm);
    exec_init(S);
    cons_init(&S->cons, &S->fault, &S->chnl, &S->vm);
    diag_init(&S->diag, &S->cons);
    srcr_init(&S->srcr, &S->cons, &S->chnl, &S->repl.fline);
    file_init(&S->file, &S->allo, &S->chnl, &S->strg, &S->cons, &S->fault);
    S->comp.fault = &S->fault;
    S->comp.allo = &S->allo;
    modu_init(&S->modu, &S->fault);
    natv_init(&S->natv, &S->fault);
    S->comp.modu = &S->modu;
    S->comp.natv = &S->natv;
    S->comp.impt = &S->impt;
    S->comp.prog = &S->prog;
    S->comp.symb = &S->symb;
    S->comp.scop = &S->scop;
    S->comp.decl = &S->decl;
    S->comp.blok = &S->blok;
    S->comp.code = &S->code;
    S->comp.ast = &S->ast;
    S->comp.ctrl = &S->ctrl;
    S->comp.expr = &S->expr;
    S->comp.link = &S->link;
    S->comp.list = &S->list;
    S->comp.cons = &S->cons;
    S->comp.vm = &S->vm;
    S->comp.dbug = &S->dbug;
    S->comp.run = session_comp_run(S);
    comp_init(&S->comp);
    flow_init(S);
    stmt_init(S);
    S->allo.cache =
        allo_base(&S->allo, allo_new(&S->allo, CACHE_FIRST, nullptr)); /* then the store */
    prog_new(&S->prog, &S->allo, &S->fault, session_prog_user(S));

    if (fe->regions)
        fe->regions(S); /* the variables, names last: they grow while code holds their cells */
    symb_new(&S->symb, &S->allo, &S->fault);
    S->vm.ftop = S->vm.frames;
    prog_line(&S->prog, DLINE)->len = 0; /* the direct line: len 0, num FF00h */
    prog_line(&S->prog, DLINE)->num = 0xFF00;
    S->rndm.seed = RNDM_SEED0; /* QBasic's first seed */
    fmth_init();
    psys_init(&S->vm.stop, SF_BRK);
    syst_init(&S->syst);
    session_new(S);
    fault_pop(&S->fault);
}
