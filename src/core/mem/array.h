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

enum { ADIMS = 64 }; /* the subscript buffers' words */
static_assert(MAXDIMS <= ADIMS && MAXDIMS <= DIM_COUNT);

typedef struct Arry {
    Allo *allo;             /* the headers' region; the data from allo.bot up to allo.arrays */
    Fault *fault;           /* Subscript out of range, Out of memory, ... */
    Symb *symb;             /* the arrays' variables */
    const Decl *decl;       /* the TYPEs' sizes, OPTION BASE */
    Strg *strg;             /* collected for room; a string element's block follows it */
    const Addr *fp;         /* the running call's frame (vm.fp): its arrays' cells */
    Addr hdr;               /* past the headers (mm.ahd) */
    int32_t subs[ADIMS];    /* the subscripts (g_dimbuf) */
    uint32_t counts[ADIMS]; /* the counts (g_cntbuf) */
    int32_t lbounds[ADIMS]; /* the lower bounds (g_lbbuf) */
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
/* Array id's elements' ET_*: ET_REC + t a TYPE's, ET_I4 an INT's (VS bytes), ours. */
static inline uint32_t arry_var_et(const Symb *const sy, const uint16_t id)
{
    const Var *const v = symb_var(sy, id);

    if (v->type >= ET_REC)
        return v->type;
    if (v->info & SK_DBL)
        return ET_R8;
    if (v->type == ET_SH || v->type == ET_BY)
        return v->type;

    return (v->info & VK_MASK) == VK_STR ? ET_SD : (v->info & VK_MASK) == VK_INT ? ET_I4 : ET_R4;
}

/* An element's bytes by its ET_*. */
static inline uint32_t arry_et_size(const Decl *const d, const uint32_t et)
{
    switch (et) {
    case ET_R8:
        return 8;

    case ET_SH:
        return 2;

    case ET_BY:
        return 1;

    case ET_SD:
        return DSZ;

    default:
        return et >= ET_REC ? decl_type(d, et - ET_REC)->size : VS;
    }
}

/* Array id's element size: inline, an element's address takes it every time. */
static inline uint32_t arry_elem_size(const Symb *const sy, const Decl *const d, const uint16_t id)
{
    return arry_et_size(d, arry_var_et(sy, id));
}

static inline uint32_t arry_esize(const Arry *const ay, const uint16_t id)
{
    return arry_elem_size(ay->symb, ay->decl, id);
}
/* A 1-D header in a frame: its bytes to the data. */
enum { ARRY_FRAME_HDR1 = (sizeof(ArrayHdr) + 2 * sizeof(uint32_t) + 7) & ~7 };
/* A header of dims dimensions in a frame: its bytes to the data, 8-aligned. */
uint32_t arry_frame_hdr(uint8_t dims);
/* Header h is the heap's (arry_new's), not a frame's. */
bool arry_heap(const Arry *ay, Addr h);
/* arry.counts[0..n): arry.subs' + 1. Illegal function call: a subscript under 0. */
void arry_counts(Arry *ay, uint8_t n);
/* DIM: array id made, its counts arry.subs[0..n) + 1; dyn a REDIM's. Illegal function call: a
   subscript under 0. Duplicate definition: it exists. Out of memory. */
void arry_dim(Arry *ay, uint16_t id, uint8_t n, bool dyn);
/* The array fields of the n records of TYPE td at data dynamic and empty, ours. */
void arry_empty_cells(Addr data, uint32_t n, const TypeDef *td);
/* DIM a(), ours: array id dynamic and empty until a REDIM, as ERASE leaves one; keep: a STATIC's
   with elements kept. Duplicate definition: it has elements. */
void arry_empty(Arry *ay, uint16_t id, bool keep);
/* a() = b(), ours: cell d gets s's bounds and a copy of its elements, a dynamic array after, its
   old one freed. Duplicate definition: d a DIM's with elements. Out of memory. */
void arry_copy(Arry *ay, Vm *vm, ArrSlot *d, const ArrSlot *s);
/* arry_copy's copy into the empty cell d, ours: made at the heap's end, nothing moves. */
void arry_copy_into(Arry *ay, ArrSlot *d, const ArrSlot *s);
/* A record of TYPE td copied from src to dst, ours: dst's STRINGs and arrays made its own (its old
   ones the caller's to kill). Nothing moves. */
void arry_copy_rec(Arry *ay, Addr dst, Addr src, const TypeDef *td);
/* A record of TYPE td moved from src to dst, ours: its STRINGs' and arrays' owners follow; src's
   cells let go. */
void arry_move_rec(const Arry *ay, Addr dst, Addr src, const TypeDef *td);
/* a() = f(..), ours: cell d takes the array of cell s, an array result's, its old one freed; s is
   left empty. Duplicate definition: d a DIM's with elements. */
void arry_move(Arry *ay, Vm *vm, ArrSlot *d, ArrSlot *s);
/* An array result emptied as its FUNCTION starts, ours: cell s's array freed, s dynamic and empty. */
void arry_reset(Arry *ay, Vm *vm, ArrSlot *s);
/* An array of et elements made in cell s, ours: its header at arry.hdr, its owner s, arry.counts
   and arry.lbounds [0..n), its data zeroed at allo.arrays; dyn a REDIM's. Returns the header. Out
   of memory. */
Addr arry_make(Arry *ay, ArrSlot *s, uint32_t et, uint8_t n, bool dyn);
/* arry_make for array id in its cell. */
Addr arry_new(Arry *ay, uint16_t id, uint8_t n, bool dyn);
/* The data's bytes of the array with header hdr. */
uint32_t arry_size(const Arry *ay, Addr hdr);
/* Header h's array dead, ours, and the arrays its records hold: arry_sweep removes them. A frame's
   is not the heap's: it goes with the frame, its records' arrays here. */
void arry_kill(const Arry *ay, Addr h);
/* The arrays the records of header h's array hold dead, ours; the array itself kept. */
void arry_kill_elems(const Arry *ay, Addr h);
/* The dead arrays removed, ours: what follows each moves down, and the headers' data, their
   owners in the moved data, the moved headers' cells, the moved string blocks' owners and the
   parameters' cells in the moved data follow. A cell held in the heap moves: not to be used after. */
void arry_sweep(Arry *ay, Vm *vm);
/* Cell s's array dropped, s emptied: arry_kill, then arry_sweep. */
void arry_free(Arry *ay, Vm *vm, ArrSlot *s);
/* REDIM PRESERVE: each element of the header old that fresh has too, by its subscripts, copied;
   a string's block moved (vm's ls_mov), a record's STRINGs' and arrays' owners too. */
void arry_keep(Arry *ay, Addr old, Addr fresh);
#endif
