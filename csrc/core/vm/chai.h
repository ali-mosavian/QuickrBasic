/* CHAIN and COMMON (src/vm/chain.asm). COMMON names its variables in order as a run reaches them;
   CHAIN keeps their values, loads its file (repl_load) and runs it, where each COMMON takes the
   next value by position, an array with its bounds. Files stay open. In the editor CHAIN is
   Advanced feature unavailable. K_CMN, K_CHAIN are K handlers (vm/ops.def). */
#ifndef CORE_VM_CHAI_H
#define CORE_VM_CHAI_H
#include "vm/exec.h"

/* chn_start: a run's start: no COMMON named; CHAIN's values for this run only. */
void chai_start(void);
#endif
