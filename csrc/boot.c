/* The start: the arena, its fixed regions, the blocks, the parts' state.
 *
 * # How it works
 * boot sets the error handler, takes the arena from the platform (exiting with "Out of memory" when
 * it gives too little), then runs each module's init in the asm's order (main.asm's rt_start):
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
#include "mem/program.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "num/fpmath.h"
#include "num/random.h"
#include "plat/pfil.h"
#include "plat/pmem.h"
#include "plat/psys.h"
#include "vm/exec.h"
#include "vm/flow.h"
#include "vm/stmts.h"
#include "vm/system.h"

void boot(Session *const S, const FrontEnd *const fe)
{
    S->repl.fe = fe;
    S->fault.raise = repl_fault;
    scop_init(&S->scop, &S->fault);
    decl_init(&S->decl, &S->fault);
    S->names.intern = repl_intern;
    lexr_init(&S->lexr, &S->fault, &S->names);
    S->srcr.src.more = srcr_more;
    stmt_usng_init(S);
    Addr lo, hi;

    if (!pmem_init(HMIN, &lo, &hi)) {
        static const char msg[] = "Out of memory\r\n";
        uint32_t put;
        pfil_write(PFIL_STDERR, msg, sizeof msg - 1, &put);
        psys_exit(1);
    }

    allo_init(
        &S->allo,
        &S->fault,
        lo,
        hi
    ); /* the arrays' headers, the control stack, the POKE memory */
    strg_init(&S->strg, &S->allo, &S->fault, &S->prog);
    arry_init(&S->arry, &S->allo, &S->fault, &S->symb, &S->decl, &S->strg);
    exec_init(S);
    cons_init(S);
    file_init(S);
    comp_init(S);
    flow_init(S);
    stmt_init(S);
    S->allo.cache = allo_base(&S->allo, allo_new(&S->allo, 0x10000, nullptr)); /* then the store */
    prog_new(&S->prog, &S->allo, &S->fault, session_prog_user(S));

    if (fe->regions)
        fe->regions(S); /* the variables, names last: they grow while code holds their cells */
    symb_new(&S->symb, &S->allo, &S->fault);
    S->vm.nframes = 0;
    prog_line(&S->prog, DLINE)->len = 0; /* the direct line: len 0, num FF00h */
    prog_line(&S->prog, DLINE)->num = 0xFF00;
    S->rndm.seed = RNDM_SEED0; /* QBasic's first seed */
    fmth_init();
    psys_init(&S->vm.stop, SF_BRK);
    syst_init(S);
    session_new(S);
}
