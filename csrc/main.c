/* The start (main.asm's rt_start): the arena, its fixed regions, the blocks, the parts' state,
   then the READY loop. */
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

int main(void)
{
    Addr lo, hi;
    if (!pmem_init(HMIN, &lo, &hi)) {
        static const char msg[] = "Out of memory\r\n";
        uint32_t put;
        pfil_write(PFIL_STDERR, msg, sizeof msg - 1, &put);
        psys_exit(1);
    }
    allo_init(lo, hi); /* the arrays' headers, the control stack, the POKE memory */
    strg_init();
    exec_init();
    cons_init();
    file_init();
    comp_init();
    flow_init();
    stmt_init();
    allo.cache = allo_base(allo_new(0x10000, nullptr)); /* the cache, then the program store */
    allo_new(0x10000, prog_place);
    prog_place();
#if IDE
    lmap_init(); /* the variables, names last: they grow while code holds their cells */
#endif
    symb_new();
    exec.cstk = allo.cs_top;
    prog_line(DLINE)->len = 0; /* the direct line: len 0, num FF00h */
    prog_line(DLINE)->num = 0xFF00;
    rndm.seed = RNDM_SEED0; /* QBasic's first seed */
    fmth_init();
    psys_init(&exec.stop, SF_BRK);
    syst_init();
    prog_init();
    repl_main();
}
