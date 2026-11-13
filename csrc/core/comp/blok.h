/* Block statements paired across lines before a run (comp/block.asm). A position is a statement's
   offset in the program store; blok_pair walks every statement in program order with a stack of
   the open blocks and gives each block statement a Block, in that order (blok_find's binary
   search). A target is a position * 4 + a mode: 0 the statement, 1 past the JMP it starts with, 2
   the statement after it; the direct line's (PDIR) its body offset's. A jump op holds it as a
   TJUMP (vm), and the address it resolves to.
   SUB and FUNCTION .. END give blok_procs an entry; DEF FN is a FUNCTION (PF_DEF), to END DEF or,
   with = e, its own statement. */
#ifndef COMP_BLOK_H
#define COMP_BLOK_H
#include "defs.h"

/* A block statement's entry. */
typedef struct PACKED {
    uint32_t pos;   /* its position */
    uint32_t next;  /* the target its false test, or its loop, goes to */
    uint32_t done;  /* the target leaving the block */
    uint16_t link;  /* the next entry + 1 of its chain (ELSE, CASE, EXIT) */
    uint16_t aux;   /* SELECT: depth, (its value's T_* | 1) << 8; CASE: its SELECT's; EXIT: BK_* */
    uint8_t kind;   /* BK_* */
    uint8_t pad[15];
} BlockEnt;
static_assert(sizeof(BlockEnt) == 32);

/* A SUB, FUNCTION or DEF FN. */
typedef struct PACKED {
    uint16_t name;
    uint8_t kind;   /* PK_SUB, PK_FUNC */
    uint8_t flags;  /* PF_* */
    uint32_t entry; /* its body: a target */
    uint16_t first; /* its first line, its last (program store offsets) */
    uint16_t last;
    uint16_t npar;  /* its parameters: their variables in blok_params from par */
    uint16_t result; /* a FUNCTION's result variable */
    uint16_t par;
    uint16_t pad0;
    uint32_t skip;  /* past its END: a target */
    uint8_t pad[8];
} Proc;
static_assert(sizeof(Proc) == 32);

enum { NBST = 64 }; /* blocks open at most */
/* An open block: its kind, its opener's entry, its last clause (ELSED: an ELSE, no false test
   after it), its chain's head + 1. */
typedef struct {
    uint8_t kind;
    uint16_t open, last, chain;
} Open;

typedef struct {
    uint16_t nblocks; /* nblk */
    uint16_t depth;   /* bsp: the scan's stack */
    uint32_t line;    /* scanln: the line scanned */
    uint16_t nprocs;  /* nprc: procedures + 1 */
    uint16_t nparams; /* nppar */
    uint16_t proc;    /* cprc: the procedure the scan is in, 0 none */
    Open open[NBST];  /* the scan's open blocks */
    const uint8_t *body; /* (EBP) the body scanned */
    uint32_t off;     /* (ECX) the statement's offset in it */
    bool isdef;       /* g_bkdef: the procedure is a DEF FN's */
    BlockEnt blocks[NBLK]; /* g_blk */
    Proc procs[NPROC];     /* g_prc, from 1 */
    uint16_t params[NPPAR]; /* g_ppar: parameters' variables */
    uint8_t byval[NPPAR];   /* g_pbyv: 1 BYVAL */
} Blok;
extern Blok blok;


void blok_pair(void);           /* cm_blocks: pair the program's blocks; FOR without NEXT .. */
int32_t blok_find(uint32_t line, uint32_t off); /* the entry of line's statement at body offset
                                                   off; -1 none */
uint32_t blok_lineof(uint32_t pos, uint32_t *off); /* position pos: its line, body offset */
uint8_t blok_scope(uint32_t line); /* bk_proc: the procedure line is in, 0 none; the scopes its */
#endif
