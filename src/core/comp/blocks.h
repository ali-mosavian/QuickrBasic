/* Block pairing's interface. How it works: blocks.c. */
#ifndef COMP_BLOCKS_H
#define COMP_BLOCKS_H
#include "defs.h"
#include "fault.h"
#include "mem/program.h"
#include "mem/symbols.h"
#include "util/arena.h"

/* A block statement's entry. */
typedef struct BlockEnt BlockEnt;
struct BlockEnt {
    uint32_t pos;   /* its position */
    uint32_t next;  /* the target its false test, or its loop, goes to */
    uint32_t done;  /* the target leaving the block */
    BlockEnt *link; /* the next of its chain (ELSE, CASE, EXIT) */
    BlockEnt *sel;  /* CASE: its SELECT; NEXT: its FOR */
    uint16_t aux;   /* SELECT: depth, (its value's T_* | 1) << 8; EXIT: BK_* */
    uint8_t kind;   /* BK_* */
};

/* A parameter: its name's position, the variable link_start declares for it. */
typedef struct {
    uint32_t at;
    uint16_t var;
    bool byval;
} Param;

/* A SUB, FUNCTION or DEF FN. */
typedef struct {
    uint32_t at; /* its name's position */
    uint16_t name;
    uint8_t kind;   /* PK_SUB, PK_FUNC */
    uint8_t flags;  /* PF_* */
    uint32_t entry; /* its body: a target */
    uint32_t first; /* its first line, its last (program store offsets) */
    uint32_t last;
    uint16_t npar;
    uint16_t result; /* a FUNCTION's result variable */
    uint16_t nframe; /* its frame's cells (comp/frame.c) */
    Param *params;
    uint32_t skip; /* past its END: a target */
} Proc;

/* The program's blocks and procedures, as blok_pair found them; its arena holds them. */
typedef struct {
    Arena arena;
    BlockEnt **blocks; /* by position */
    uint16_t nblocks;
    Proc **procs;    /* by number, from 1 */
    uint16_t nprocs; /* procedures + 1 */
} Blok;

/* cm_blocks: pair the program's blocks; FOR without NEXT .. raise through f. */
void blok_pair(Blok *b, const Prog *pg, const Symb *sy, Fault *f);
/* The entry of line's statement at body offset off; null none. */
BlockEnt *blok_find(const Blok *b, uint32_t line, uint32_t off);
/* Procedure i; 0, or none yet, an empty one: the module. */
const Proc *blok_proc(const Blok *b, uint8_t i);
/* bk_proc: the procedure line is in, 0 none. */
uint8_t blok_procof(const Blok *b, uint32_t line);
#endif
