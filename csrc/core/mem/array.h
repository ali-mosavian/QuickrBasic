/* Arrays' interface. How it works: array.c. */
#ifndef MEM_ARRAY_H
#define MEM_ARRAY_H
#include "defs.h"

enum { ADIMS = 64 };      /* the subscript buffers' words */
enum : Addr { AREP = 1 }; /* arry_free's rep: a mark arry_rep replaces */

typedef struct {
    Addr end;               /* past the arrays' data (mm.aend) */
    Addr hdr;               /* past the headers (mm.ahd) */
    uint16_t subs[ADIMS];   /* the subscripts (g_dimbuf) */
    uint16_t counts[ADIMS]; /* the counts (g_cntbuf) */
    int16_t lbounds[ADIMS]; /* the lower bounds (g_lbbuf) */
} Arry;

/* Array id's element at arry.subs[0..n); a new array has every subscript OPTION BASE to 10.
   Subscript out of range: n not the array's, a subscript out of its bounds, or ERASE freed it.
   Out of memory. */
Addr arry_ref(Session *S, uint16_t id, uint8_t n);
/* Subscript out of range (E_BS). */
[[noreturn]]
void arry_bad_sub(Session *S);
/* Array id's element size. */
uint32_t arry_esize(Session *S, uint16_t id);
/* DIM: array id made, its counts arry.subs[0..n) + 1; dyn a REDIM's. Illegal function call: a
   subscript under 0. Duplicate definition: it exists. Out of memory. */
void arry_dim(Session *S, uint16_t id, uint8_t n, bool dyn);
/* Array id made: its header at arry.hdr, arry.counts and arry.lbounds [0..n), its data zeroed at
   arry.end; dyn a REDIM's. Returns the header. Out of memory. */
Addr arry_new(Session *S, uint16_t id, uint8_t n, bool dyn);
/* The data's bytes of array id with header hdr. */
uint32_t arry_size(Session *S, uint16_t id, Addr hdr);
/* Array id dropped: its header and data go, what follows moves down, and the headers' data, the
   moved string blocks' owners, the headers in Var.vt and on the save stack, and the parameters'
   cells in the moved data follow; a header kept that was this one's becomes rep (0, or AREP).
   Returns the header's bytes (g_afhs). */
uint32_t arry_free(Session *S, uint16_t id, Addr rep);
/* REDIM PRESERVE: each element of the header old that fresh has too, by its subscripts, copied;
   a string's block moved (vm's ls_mov). */
void arry_keep(Session *S, uint16_t id, Addr old, Addr fresh);
/* The references arry_free marked AREP: to array id's header. */
void arry_rep(Session *S, uint16_t id);
#endif
