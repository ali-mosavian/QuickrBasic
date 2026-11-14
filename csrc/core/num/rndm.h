/* RND (float/ftr.asm): QBasic's 24-bit generator. */
#ifndef NUM_RNDM_H
#define NUM_RNDM_H
#include "sngl.h"

typedef struct {
    uint32_t seed; /* 24 bits; RANDOMIZE (vm/sys) sets its middle word */
} Rndm;

enum : uint32_t { RNDM_SEED0 = 0x50000 }; /* QBasic's first seed, set at start (main) */

/* RND(x): 0 repeats the last, a negative x seeds from its bits first; then the next. */
Sngl rndm_rnd(Session *S, Sngl x);
#endif
