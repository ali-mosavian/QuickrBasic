/* QBasic's system routines (src/vm/sys.asm): TIMER, DATE$, TIME$ and their statements, INKEY$,
   SLEEP, COMMAND$, ENVIRON and ENVIRON$, SHELL, SETMEM, ERDEV, RANDOMIZE. Their CALL routines
   are K handlers (vm/ops.def). */
#ifndef CORE_VM_SYST_H
#define CORE_VM_SYST_H
#include "vm/exec.h"

enum { ENVSZ = 4096, CMDSZ = 128 };

typedef struct {
    /* g_env: the environment, copied at the start: NAME=value 0 each, a 0 after the last;
       ENVIRON changes the copy, as QBasic's own; SHELL's child gets it. */
    char env[ENVSZ];
    char cmd[CMDSZ]; /* g_cmd: COMMAND$, the command line after the program's file, upper case */
    uint16_t cmdlen; /* g_cmdn */
} Syst;

/* sys_init: the environment and the command line, from the platform's (psys). */
void syst_init(Session *S);
/* sx_sret: cons.sbuf's first len chars pushed as a temporary string. */
void syst_ret(Session *S, uint32_t len);
#endif
