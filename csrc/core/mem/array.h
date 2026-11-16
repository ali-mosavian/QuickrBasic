/* Arrays' interface. How it works: array.c. */
#ifndef MEM_ARRAY_H
#define MEM_ARRAY_H
#include "defs.h"
#include "fault.h"
#include "comp/declare.h"
#include "mem/alloc.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "vm/exec.h"

enum { ADIMS = 64 };      /* the subscript buffers' words */
enum : Addr { AREP = 1 }; /* arry_free's rep: a mark arry_rep replaces */

typedef struct Arry {
    Allo *allo;             /* the headers' region; the data from allo.bot up to allo.arrays */
    Fault *fault;           /* Subscript out of range, Out of memory, ... */
    Symb *symb;             /* the arrays' variables */
    const Decl *decl;       /* the TYPEs' sizes, OPTION BASE */
    Strg *strg;             /* collected for room; a string element's block follows it */
    const Addr *fp;         /* the running call's frame (vm.fp): its arrays' cells */
    Addr hdr;               /* past the headers (mm.ahd) */
    uint16_t subs[ADIMS];   /* the subscripts (g_dimbuf) */
    uint16_t counts[ADIMS]; /* the counts (g_cntbuf) */
    int16_t lbounds[ADIMS]; /* the lower bounds (g_lbbuf) */
} Arry;

/* ay's links set. */
void arry_init(Arry *ay, Allo *al, Fault *f, Symb *sy, const Decl *d, Strg *sg, const Addr *fp);
/* Array id's cell in the running call: its own, a local's in the frame, a parameter's its
   argument's. */
ArrSlot *arry_slot(const Arry *ay, uint16_t id);
/* Array id's element at arry.subs[0..n), s its cell; a new array has every subscript OPTION BASE to 10.
   Subscript out of range: n not the array's, a subscript out of its bounds, or ERASE freed it.
   Out of memory. */
Addr arry_ref(Arry *ay, ArrSlot *s, uint16_t id, uint8_t n);
/* Subscript out of range (E_BS). */
[[noreturn]]
void arry_bad_sub(const Arry *ay);
/* Array id's element size. */
uint32_t arry_esize(const Arry *ay, uint16_t id);
/* DIM: array id made, its counts arry.subs[0..n) + 1; dyn a REDIM's. Illegal function call: a
   subscript under 0. Duplicate definition: it exists. Out of memory. */
void arry_dim(Arry *ay, uint16_t id, uint8_t n, bool dyn);
/* Array id made: its header at arry.hdr, arry.counts and arry.lbounds [0..n), its data zeroed at
   allo.arrays; dyn a REDIM's. Returns the header. Out of memory. */
Addr arry_new(Arry *ay, uint16_t id, uint8_t n, bool dyn);
/* The data's bytes of array id with header hdr. */
uint32_t arry_size(const Arry *ay, uint16_t id, Addr hdr);
/* Array id dropped: its header and data go, what follows moves down, and the headers' data, the
   moved string blocks' owners, the headers in the arrays' cells (vm's frames' too), and the
   parameters' cells in the moved data follow; a header kept that was this one's becomes rep (0,
   or AREP).
   Returns the header's bytes (g_afhs). */
uint32_t arry_free(Arry *ay, Vm *vm, uint16_t id, Addr rep);
/* REDIM PRESERVE: each element of the header old that fresh has too, by its subscripts, copied;
   a string's block moved (vm's ls_mov). */
void arry_keep(Arry *ay, uint16_t id, Addr old, Addr fresh);
/* The references arry_free marked AREP: to array id's header. */
void arry_rep(Arry *ay, Vm *vm, uint16_t id);
#endif
