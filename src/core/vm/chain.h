/* CHAIN and COMMON's interface. How it works: chain.c. */
#ifndef CORE_VM_CHAI_H
#define CORE_VM_CHAI_H
#include "vm/exec.h"
typedef struct Chai Chai;

/* chn_start: a run's start: no COMMON named (sy's variables'); CHAIN's values for this run only. */
void chai_start(Chai *ch, Symb *sy);
enum { CHAIN_NAME_MAX = 120 };

/* CHAIN's state: COMMON's values carried to the next program */
typedef struct Chai {
    const uint8_t *next; /* g_chr: the next value to take */
    uint8_t *end;        /* g_che: past the last */
    bool chained;        /* g_chf: CHAIN's run starting */
    uint32_t n, ncap;    /* g_cmn: the ids COMMON named so far in this run */
    uint32_t *id;        /* g_cmid, ncap of them */
    uint8_t *buf;        /* g_chb: cap bytes, len of them the values */
    uint32_t len, cap;
} Chai;

#endif
