/* Strings' interface. How it works: string.c. */
#ifndef MEM_STRING_H
#define MEM_STRING_H
#include "defs.h"
#include "fault.h"
#include "mem/alloc.h"
#include "mem/program.h"

enum { NEAR_MAX = 255 };

typedef struct {
    Allo *allo;         /* the heap: the blocks from allo.strings up to its top */
    Fault *fault;       /* String formula too complex, String too long, Out of memory */
    const Prog *prog;   /* a value in the program's text is kept there */
    Addr tmp;           /* the temporaries' Descs, DSZ * NTMP bytes (g_tmpd, sg.tmplin) */
    uint32_t tmp_used;  /* the temporaries in use, DSZ bytes each (sg.tsp) */
    uint32_t tmp_floor; /* a procedure's floor of tmp_used (sg.tspb) */
    Cell *const *sp;    /* vm.sp: the data stack's Descs live at or above it (vm/stack.c) */
    uint8_t near_buf[NEAR_MAX + 1]; /* strg_near's */
} Strg;

/* The comparison's result (strg_cmp). */
enum : uint8_t { SCMP_GT = 1, SCMP_EQ = 2, SCMP_LT = 4 };

/* The temporaries' region carved from al. */
void strg_init(Strg *sg, Allo *al, Fault *f, const Prog *pg);
/* The length and chars of the Desc at desc; the top temporary is popped. */
Desc strg_get(Strg *sg, Addr desc);
/* The Desc at desc, the temporaries untouched. */
static inline Desc strg_peek(const Addr desc)
{
    Desc s;
    memcpy(&s, at(desc), DSZ);
    return s;
}
/* A new temporary for s. String formula too complex (E_ST): NTMP in use. */
Addr strg_push(Strg *sg, Desc s);
/* A heap block for len chars, no owner yet; returns its chars. Collects when full. Out of
   memory. */
Addr strg_alloc(Strg *sg, uint16_t len);
/* A new temporary for the block strg_alloc made, which it owns. */
Addr strg_new(Strg *sg, Addr chars, uint16_t len);
/* The block of len chars at chars owned by the Desc at owner. */
void strg_own(Addr chars, uint16_t len, Addr owner);
/* Collect the heap: the live blocks slide up, their owners follow, the dead go; low set. */
void strg_gc(Strg *sg);
/* size bytes from the heap's top, the strings collected and slid down under them: false, no room.
   strg_give_top gives them back, the strings slid up. */
bool strg_take_top(Strg *sg, uint32_t size);
void strg_give_top(Strg *sg, uint32_t size);
/* Store value in the Desc at desc: pointed at when empty or in the program store, a temporary's
   own block taken over, else copied. Out of memory. */
void strg_set(Strg *sg, Addr value, Addr desc);
/* s's first 255 chars, a 0 after them, in a buffer of strg's. */
const uint8_t *strg_near(Strg *sg, Desc s);
/* A READ or INPUT item at *pos (spaces before it skipped): "quoted", or up to ',' ':' or the end;
   a temporary for its text in place. *pos at its end (past a closing quote). */
Addr strg_item(Strg *sg, Addr *pos);
/* Past the closing quote at p, if p is not at the 0. */
const uint8_t *strg_skipq(const uint8_t *p);
/* The text at *p up to a 0, stop1 or stop2: its length; *p at the stop. */
uint32_t strg_span(const uint8_t **p, uint8_t stop1, uint8_t stop2);
/* The length in the Desc at desc (no pop). */
uint16_t strg_len(Addr desc);
/* left + right, a new temporary. String too long (E_LS): over SMAX. */
Addr strg_cat(Strg *sg, Addr left, Addr right);
/* left against right, a prefix less: SCMP_*. */
uint8_t strg_cmp(Strg *sg, Addr left, Addr right);
/* A new temporary of desc's chars from start (0 the first), most of them. */
Addr strg_sub(Strg *sg, Addr desc, uint16_t start, uint16_t most);
/* len chars from src copied to the block at chars (strg_alloc's): a new temporary. */
Addr strg_tods(Strg *sg, Addr chars, Addr src, uint16_t len);
/* A new temporary for a copy of the len chars at text (the asm's g_sbuf's). */
Addr strg_text(Strg *sg, const uint8_t *text, uint16_t len);
/* The Desc at from to to (vm's ls_mov): a heap block's owner follows. */
void strg_move(const Strg *sg, Addr from, Addr to);
/* A record copied from src to dst, ours: the n Descs at offs in dst made their own copies. Out of
   memory. */
void strg_copy_owned(Strg *sg, Addr dst, Addr src, const uint16_t *offs, uint32_t n);
/* A record moved from src to dst, ours: the n Descs at offs, their blocks' owners follow. */
void strg_move_owned(const Strg *sg, Addr dst, Addr src, const uint16_t *offs, uint32_t n);
#endif
