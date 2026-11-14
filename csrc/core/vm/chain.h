/* CHAIN and COMMON (src/vm/chain.asm). COMMON names its variables in order as a run reaches them;
   CHAIN keeps their values, loads its file (repl_load) and runs it, where each COMMON takes the
   next value by position, an array with its bounds. Files stay open. In the editor CHAIN is
   Advanced feature unavailable. K_CMN, K_CHAIN are K handlers (vm/ops.def). */
#ifndef CORE_VM_CHAI_H
#define CORE_VM_CHAI_H
#include "vm/exec.h"

/* chn_start: a run's start: no COMMON named; CHAIN's values for this run only. */
void chai_start(Session *S);
enum { NCMN = 512, CHAIN_BUF_SIZE = 0x10000, CHAIN_NAME_MAX = 120 };

/* CHAIN's state: COMMON's values carried to the next program */
typedef struct {
    const uint8_t *next;         /* g_chr: the next value to take */
    uint8_t *end;                /* g_che: past the last */
    bool chained;                /* g_chf: CHAIN's run starting */
    uint32_t n;                  /* g_cmn: the ids COMMON named so far in this run */
    uint16_t id[NCMN];           /* g_cmid */
    uint8_t buf[CHAIN_BUF_SIZE]; /* g_chb */
} Chai;

#endif
