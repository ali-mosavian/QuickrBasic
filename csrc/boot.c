/* The start (main.asm's rt_start): the arena, its fixed regions, the blocks, the parts' state. */
#include "boot.h"
#include "session.h"
#include "comp/comp.h"
#include "io/cons.h"
#include "io/file.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/prog.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "num/fmth.h"
#include "num/rndm.h"
#include "plat/pfil.h"
#include "plat/pmem.h"
#include "plat/psys.h"
#include "vm/exec.h"
#include "vm/flow.h"
#include "vm/stmt.h"
#include "vm/syst.h"

#if IDE
#include "ui/lmap.h"
#endif

void boot(Session *const S)
{
    session_reset(S);
    Addr lo, hi;
    if (!pmem_init(HMIN, &lo, &hi)) {
        static const char msg[] = "Out of memory\r\n";
        uint32_t put;
        pfil_write(PFIL_STDERR, msg, sizeof msg - 1, &put);
        psys_exit(1);
    }
    allo_init(S, lo, hi); /* the arrays' headers, the control stack, the POKE memory */
    strg_init(S);
    exec_init(S);
    cons_init(S);
    file_init(S);
    comp_init(S);
    flow_init(S);
    stmt_init(S);
    S->allo.cache = allo_base(&S->allo, allo_new(S, 0x10000, nullptr)); /* the cache, then the program store */
    allo_new(S, 0x10000, prog_place);
    prog_place(S);
#if IDE
    lmap_init(S); /* the variables, names last: they grow while code holds their cells */
#endif
    symb_new(S);
    S->vm.nframes = 0;
    prog_line(&S->prog, DLINE)->len = 0; /* the direct line: len 0, num FF00h */
    prog_line(&S->prog, DLINE)->num = 0xFF00;
    S->rndm.seed = RNDM_SEED0; /* QBasic's first seed */
    fmth_init();
    psys_init(&S->vm.stop, SF_BRK);
    syst_init(S);
    prog_init(S);
}
