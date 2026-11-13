/* Strings and their heap (src/mem/str.asm).

   A string value is the Addr of its Desc: a variable's cell, an array element, a record's field or
   a temporary. The temporaries are NTMP Descs at strg.tmp, tmp_used bytes of them in use. A Desc's
   ptr may point into the program store, a text buffer in the arena or a heap block. The heap's
   strings run down from allo.top to low; a block is the chars, then a StrTail (len, owner: the
   Desc's Addr), live while that Desc points at the chars. At most SMAX chars. */
#ifndef MEM_STRG_H
#define MEM_STRG_H
#include "defs.h"

typedef struct {
    Addr tmp;           /* the temporaries' Descs, DSZ * NTMP bytes (g_tmpd, sg.tmplin) */
    uint16_t tmp_used;  /* the temporaries in use, DSZ bytes each (sg.tsp) */
    uint16_t tmp_floor; /* a procedure's floor of tmp_used (sg.tspb) */
    Addr low;           /* the lowest string block (mm.sfr) */
} Strg;
extern Strg strg;

/* The comparison's result (strg_cmp). */
enum : uint8_t { SCMP_GT = 1, SCMP_EQ = 2, SCMP_LT = 4 };

/* The temporaries' region carved. */
void strg_init(void);
/* The length and chars of the Desc at desc; the top temporary is popped. */
Desc strg_get(Addr desc);
/* A new temporary for s. String formula too complex (E_ST): NTMP in use. */
Addr strg_push(Desc s);
/* A heap block for len chars, no owner yet; returns its chars. Collects when full. Out of
   memory. */
Addr strg_alloc(uint16_t len);
/* A new temporary for the block strg_alloc made, which it owns. */
Addr strg_new(Addr chars, uint16_t len);
/* The block of len chars at chars owned by the Desc at owner. */
void strg_own(Addr chars, uint16_t len, Addr owner);
/* Collect the heap: the live blocks slide up, their owners follow, the dead go; low set. */
void strg_gc(void);
/* Store value in the Desc at desc: pointed at when empty or in the program store, a temporary's
   own block taken over, else copied. Out of memory. */
void strg_set(Addr value, Addr desc);
/* s's first 255 chars, a 0 after them, in a buffer of strg's. */
const uint8_t *strg_near(Desc s);
/* Print s's chars through cons (asm io_putc), 255 at a time. */
void strg_print(Desc s);
/* A READ or INPUT item at *pos (spaces before it skipped): "quoted", or up to ',' ':' or the end;
   a temporary for its text in place. *pos at its end (past a closing quote). */
Addr strg_item(Addr *pos);
/* Past the closing quote at p, if p is not at the 0. */
const uint8_t *strg_skipq(const uint8_t *p);
/* The text at *p up to a 0, stop1 or stop2: its length; *p at the stop. */
uint32_t strg_span(const uint8_t **p, uint8_t stop1, uint8_t stop2);
/* The length in the Desc at desc (no pop). */
uint16_t strg_len(Addr desc);
/* left + right, a new temporary. String too long (E_LS): over SMAX. */
Addr strg_cat(Addr left, Addr right);
/* left against right, a prefix less: SCMP_*. */
uint8_t strg_cmp(Addr left, Addr right);
/* A new temporary of desc's chars from start (0 the first), most of them. */
Addr strg_sub(Addr desc, uint16_t start, uint16_t most);
/* len chars from src copied to the block at chars (strg_alloc's): a new temporary. */
Addr strg_tods(Addr chars, Addr src, uint16_t len);
/* A new temporary for the first len chars of cons's sbuf (asm g_sbuf). */
Addr strg_buftmp(uint16_t len);
#endif
