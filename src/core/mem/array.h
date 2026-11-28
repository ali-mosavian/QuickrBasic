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
    Allo *allo;          /* the heap arrays' blocks */
    Fault *fault;        /* Subscript out of range, Out of memory, ... */
    Symb *symb;          /* the arrays' variables */
    const Decl *decl;    /* the TYPEs' sizes, OPTION BASE */
    Strg *strg;          /* collected for room; a string element's block follows it */
    Vm *vm;              /* the run: its call's frame (vm.fp), the calls that own a heap's array */
    Addr fcells;         /* the newest live field cell's header, 0 none */
    Addr dead;           /* the newest dead heap array's header, 0 none: arry_sweep's */
    Addr *holds;         /* arry_hold's sp each, the newest last */
    uint32_t nholds;     /* in use */
    uint32_t holdcap;    /* holds' room */
    int32_t subs[ADIMS]; /* the subscripts (g_dimbuf) */
    uint32_t counts[ADIMS]; /* the counts (g_cntbuf) */
    int32_t lbounds[ADIMS]; /* the lower bounds (g_lbbuf) */
} Arry;

/* ay's links set. */
void arry_init(Arry *ay, Allo *al, Fault *f, Symb *sy, const Decl *d, Strg *sg, Vm *vm);
/* No heap arrays, no field cells: their blocks gone with the heap (allo_shrink). */
void arry_clear_heap(Arry *ay);
/* Array id's cell in the running call: its own, a local's in the frame, a parameter's its
   argument's. */
ArrSlot *arry_slot(const Arry *ay, uint32_t id);
/* Array id's element at arry.subs[0..n), s its cell; a new array has every subscript OPTION BASE to 10.
   Subscript out of range: n not the array's, a subscript out of its bounds, or ERASE freed it.
   Out of memory. */
Addr arry_ref(Arry *ay, ArrSlot *s, uint32_t id, uint8_t n);
/* Subscript out of range (E_BS). */
[[noreturn]]
void arry_bad_sub(const Arry *ay);
/* Array id's elements' ET_*: ET_REC + t a TYPE's, ET_I4 an INT's (VS bytes), ours. */
static inline uint32_t arry_var_et(const Symb *const sy, const uint32_t id)
{
    const Var *const v = symb_var(sy, id);

    if (v->type >= ET_REC)
        return v->type;
    if (v->info & SK_DBL)
        return ET_R8;
    if (et_narrow(v->type))
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
    case ET_US:
        return 2;

    case ET_BY:
    case ET_SB:
        return 1;

    case ET_SD:
        return DSZ;

    default:
        return et >= ET_REC ? decl_type(d, et - ET_REC)->size : VS;
    }
}

/* Array id's element size: inline, an element's address takes it every time. */
static inline uint32_t arry_elem_size(const Symb *const sy, const Decl *const d, const uint32_t id)
{
    return arry_et_size(d, arry_var_et(sy, id));
}

static inline uint32_t arry_esize(const Arry *const ay, const uint32_t id)
{
    return arry_elem_size(ay->symb, ay->decl, id);
}
/* A 1-D header in a frame: its bytes to the data. */
enum { ARRY_FRAME_HDR1 = (sizeof(ArrayHdr) + 2 * sizeof(uint32_t) + 7) & ~7 };
/* A header of dims dimensions in a frame: its bytes to the data, 8-aligned. */
uint32_t arry_frame_hdr(uint8_t dims);
/* arry.counts[0..n): arry.subs' + 1. Illegal function call: a subscript under 0. */
void arry_counts(Arry *ay, uint8_t n);
/* DIM: array id made, its counts arry.subs[0..n) + 1. Illegal function call: a subscript under 0.
   Duplicate definition: it exists. Out of memory. */
void arry_dim(Arry *ay, uint32_t id, uint8_t n);
/* REDIM [PRESERVE, keep]: cell s's array of et elements, its counts arry.subs[0..n) + 1 and its
   lower bounds arry.lbounds; keep: the old one's elements moved in. Duplicate definition: a fixed
   one. Subscript out of range: keep, n not its dimensions. Illegal function call, Out of memory. */
void arry_redim(Arry *ay, ArrSlot *s, uint32_t et, uint8_t n, bool keep);
/* Header h is a frame's, its local's DIMmed with constant bounds (comp/frame.c), or a field cell's:
   REDIM never. */
bool arry_fixed(Addr h);
/* A list's array, ours: cell s's made cap elements of et, its first len (fast) its elements, the
   old ones kept, the old array dead for arry_sweep (an arena's till its return). Out of memory. */
void arry_relen(Arry *ay, ArrSlot *s, uint32_t et, uint32_t cap, uint32_t len);
/* The array fields of the n records of TYPE td at data dynamic and empty, ours. */
void arry_empty_cells(Addr data, uint32_t n, const TypeDef *td);
/* DIM a(), ours: array id dynamic and empty until a REDIM, as ERASE leaves one; keep: a STATIC's
   with elements kept. Duplicate definition: it has elements. */
void arry_empty(Arry *ay, uint32_t id, bool keep);
/* a(:) = b(:), ours: cell d gets s's bounds and a copy of its elements, a dynamic array after, its
   old one freed. Duplicate definition: d a DIM's with elements. Out of memory. */
void arry_copy(Arry *ay, ArrSlot *d, const ArrSlot *s);
/* Cell d's array, ours, a list of n copies of s's elements from from: d's old one freed, which
   may be s's. */
void arry_copy_part(Arry *ay, ArrSlot *d, const ArrSlot *s, uint32_t from, uint32_t n);
/* arry_copy's copy into the empty cell d, ours. */
void arry_copy_into(Arry *ay, ArrSlot *d, const ArrSlot *s);
/* A record of TYPE td copied from src to dst, ours: dst's STRINGs and arrays made its own (its old
   ones the caller's to kill). */
void arry_copy_rec(Arry *ay, Addr dst, Addr src, const TypeDef *td);
/* A record of TYPE td moved from src to dst, ours: its STRINGs' owners follow, its heap arrays
   dst's call's to free; src's cells let go. */
void arry_move_rec(const Arry *ay, Addr dst, Addr src, const TypeDef *td);
/* SWAP a, b of records of TYPE td: their bytes swapped, their STRINGs' owners following; their
   arrays in a frame or on the arena first copied to the heap. Out of memory. */
void arry_swap_rec(Arry *ay, Addr a, Addr b, const TypeDef *td);
/* a() = b() or f(..), ours: cell d takes the array of cell s, its old one freed; s is left empty.
   s's array in a frame or on the arena, d a cell its call may outlive: copied to the heap first.
   s a field cell's: copied, s kept. Duplicate definition: d a DIM's with elements. Out of memory. */
void arry_move(Arry *ay, ArrSlot *d, ArrSlot *s);
/* An array result emptied as its FUNCTION starts, ours: cell s's array freed, s dynamic and empty. */
void arry_reset(Arry *ay, ArrSlot *s);
/* An array of et elements made in cell s, ours: arry.counts and arry.lbounds [0..n), its data
   zeroed; on the running call's arena (vm/stack.c), else its header and data a block of the
   heap's (allo_take); dyn a REDIM's. Returns the header. Out of memory. */
Addr arry_make(Arry *ay, ArrSlot *s, uint32_t et, uint8_t n, bool dyn);
/* arry_make for array id in its cell. */
Addr arry_new(Arry *ay, uint32_t id, uint8_t n, bool dyn);
/* The data's bytes of the array with header hdr. */
uint32_t arry_size(const Arry *ay, Addr hdr);
/* An array's elements as LBOUND, ERASE and GET see them, ours: a header's, or a fixed array
   field's, its dimensions its field's. */
typedef struct ArrView {
    Addr data;
    uint32_t et;
    uint8_t dims;
    const ArrayHdr *hdr;   /* a header's: its counts and lower bounds */
    const FieldDim *fdims; /* a field's */
} ArrView;
/* Header h's array's view. */
ArrView arry_view(Addr h);
/* The view of the fixed array field key names (decl_fieldkey), its place at data. */
ArrView arry_view_field(const Arry *ay, Addr data, uint32_t key);
/* Its dimension i's count, lower bound; its elements' count, their bytes. */
uint32_t arry_view_count(const ArrView *v, uint32_t i);
int32_t arry_view_lbound(const ArrView *v, uint32_t i);
uint32_t arry_view_elems(const ArrView *v);
uint32_t arry_view_size(const Arry *ay, const ArrView *v);
/* A cell for the fixed array field key names (decl_fieldkey) at data, passed whole to an array
   parameter, ours: its header the field's shape. It lives while the call does, its arguments
   above vm.sp. Out of memory. */
ArrSlot *arry_field_cell(Arry *ay, Addr data, uint32_t key);
/* ERASE of a DIM'd array or a fixed array field: its elements zeroed, its records' arrays freed and
   emptied (ours). */
void arry_clear(Arry *ay, const ArrView *v);
/* Header h's array dead, ours, and the arrays its records hold: arry_sweep frees them. A frame's
   or an arena's is not the heap's: it goes with the frame, its records' arrays here. */
void arry_kill(Arry *ay, Addr h);
/* The arrays the n et elements at data hold dead, ours, a record's. */
void arry_kill_recs(Arry *ay, Addr data, uint32_t n, uint32_t et);
/* The dead arrays' blocks given back, ours, their strings' blocks dead and collected first.
   Held (arry_hold): nothing freed, the dead left to a later sweep. */
void arry_sweep(Arry *ay);
/* Code starts that may kill arrays while addresses into them wait on the stack at or above sp:
   sweeps wait till its arry_unhold, which drops the newest hold, or till an error's unwinding
   takes sp up to it (arry_unwound). Out of memory. */
void arry_hold(Arry *ay);
void arry_unhold(Arry *ay);
/* The holds at or under sp dropped: the stack unwound past them. */
void arry_unwound(Arry *ay);
/* Cell s's array dropped, s emptied: arry_kill, the arena's newest's room given back, then
   arry_sweep. */
void arry_free(Arry *ay, ArrSlot *s);
/* REDIM PRESERVE: each element of the header old that fresh has too, by its subscripts, copied;
   a string's block moved (vm's ls_mov), a record's STRINGs' too. */
void arry_keep(Arry *ay, Addr old, Addr fresh);
#endif
