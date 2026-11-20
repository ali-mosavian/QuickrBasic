/* The system routines' interface. How it works: system.c. */
#ifndef CORE_VM_SYST_H
#define CORE_VM_SYST_H
#include "vm/exec.h"

enum { ENVSZ = 4096, CMDSZ = 128 };

typedef struct {
    /* g_env: the environment, copied at the start: NAME=value 0 each, a 0 after the last;
       ENVIRON changes the copy, as QBasic's own; SHELL's child gets it. envcap bytes, grown to
       ENVSZ. */
    char *env;
    uint32_t envcap;
    char cmd[CMDSZ]; /* g_cmd: COMMAND$, the command line after the program's file, upper case */
    uint16_t cmdlen; /* g_cmdn */
} Syst;

/* sys_init: the environment and the command line, from the platform's (psys). */
void syst_init(Syst *sys);
/* The environment's value of name, case not told; null none. */
const char *syst_env(Syst *sys, const char *name);
/* sx_sret: cons.sbuf's first len chars pushed as a temporary string. */
void syst_ret(Session *S, uint32_t len);
#endif
