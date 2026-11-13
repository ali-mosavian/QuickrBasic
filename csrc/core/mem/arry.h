/* Arrays (src/mem/arr.asm). Var.vt is an array's header (ArrayHdr) in the headers' region, from
   allo.hdr_base to arry.hdr: its dimensions, its data's Addr, the count when 1-D from 0 (else 0),
   the counts, then the lower bounds (words). Its data is on the heap from allo.bot to arry.end,
   zeroed, 4-aligned: VS a number or an INT, 8 a DOUBLE, DSZ a string, a TYPE's size a record. */
#ifndef MEM_ARRY_H
#define MEM_ARRY_H
#include "defs.h"

enum { ADIMS = 64 }; /* the subscript buffers' words */
enum : Addr { AREP = 1 }; /* arry_free's rep: a mark arry_rep replaces */

typedef struct {
    Addr end;              /* past the arrays' data (mm.aend) */
    Addr hdr;              /* past the headers (mm.ahd) */
    uint16_t subs[ADIMS];  /* the subscripts (g_dimbuf) */
    uint16_t counts[ADIMS];/* the counts (g_cntbuf) */
    int16_t lbounds[ADIMS];/* the lower bounds (g_lbbuf) */
} Arry;
extern Arry arry;

/* Array id's element at arry.subs[0..n); a new array has every subscript OPTION BASE to 10.
   Subscript out of range: n not the array's, a subscript out of its bounds, or ERASE freed it.
   Out of memory. */
Addr arry_ref(uint16_t id, uint8_t n);
/* Subscript out of range (E_BS). */
[[noreturn]] void arry_bad_sub(void);
/* Array id's element size. */
uint32_t arry_esize(uint16_t id);
/* DIM: array id made, its counts arry.subs[0..n) + 1; dyn a REDIM's. Illegal function call: a
   subscript under 0. Duplicate definition: it exists. Out of memory. */
void arry_dim(uint16_t id, uint8_t n, bool dyn);
/* Array id made: its header at arry.hdr, arry.counts and arry.lbounds [0..n), its data zeroed at
   arry.end; dyn a REDIM's. Returns the header. Out of memory. */
Addr arry_new(uint16_t id, uint8_t n, bool dyn);
/* The data's bytes of array id with header hdr. */
uint32_t arry_size(uint16_t id, Addr hdr);
/* Array id dropped: its header and data go, what follows moves down, and the headers' data, the
   moved string blocks' owners, the headers in Var.vt and on the save stack, and the parameters'
   cells in the moved data follow; a header kept that was this one's becomes rep (0, or AREP).
   Returns the header's bytes (g_afhs). */
uint32_t arry_free(uint16_t id, Addr rep);
/* REDIM PRESERVE: each element of the header old that fresh has too, by its subscripts, copied;
   a string's block moved (vm's ls_mov). */
void arry_keep(uint16_t id, Addr old, Addr fresh);
/* The references arry_free marked AREP: to array id's header. */
void arry_rep(uint16_t id);
#endif
