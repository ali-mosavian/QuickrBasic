/* DOUBLE (float/dbl.asm): its D.* ops, the number reader, number text.

   A DOUBLE is an IEEE double filling a data stack Cell (`.d`), a scalar's its 8-byte cell. Each
   D op rounds its result to 53 bits once, stored to its Cell; ?OVERFLOW when it is infinity or
   NaN (dbl_chk), but for SQR, EXP, ^ and D.TOF, which keep it. In the asm's stack terms a binary
   op takes a at [esp+CELL] and b at [esp] and leaves a OP b in a's cell.

   Number text is one path: dubl_digits gives a value's digits rounded once on the x87 at 64 bits
   (pfpu_digits), dubl_fmt spells them as PRINT. SINGLE's text is its value as a DOUBLE, 7
   digits and E; DOUBLE's 16 and D, as QB45's. */
#ifndef NUM_DUBL_H
#define NUM_DUBL_H
#include "sngl.h"

enum { DUBL_TXT = 32 }; /* dubl_fmt's text at most */

/* dubl_in's kind of number by its spelling. */
enum : uint8_t { DK_ANY = 0, DK_DBL = 1, DK_SNGL = 2 };

/* D.FN's operand byte. SGN's result is an INT. */
enum : uint8_t {
    DF_ABS, DF_SGN, DF_INT, DF_FIX, DF_SQR, DF_SIN, DF_COS, DF_TAN, DF_ATN, DF_LOG, DF_EXP,
};

/* D.CMP's mask: the relations that give -1. */
enum : uint8_t { DC_GT = 1, DC_EQ = 2, DC_LT = 4 }; /* NaN against anything: DC_EQ, as fcomp */

/* Reads &H|&O digits, or [sign] digits [. digits] [E|D [sign] digits] [# | !] at *p, spaces
   between skipped (BC's VAL and INPUT); *p past it. *kind (may be nullptr): DK_DBL by D, # or
   past 7 digits (zeros after the point counted), DK_SNGL by !. ?OVERFLOW past the largest. */
double dubl_in(const uint8_t **p, uint8_t *kind);

/* v (over 0, finite) as ndig (7 or 16) digits rounded once, trailing zeros left out: dig gets
   them as '0'..'9', *dp the first's power of ten; returns their count. */
uint32_t dubl_digits(double v, uint32_t ndig, uint8_t *dig, int32_t *dp);

/* v as PRINT spells it into out (DUBL_TXT): a space or -, 16 digits at most, no 0 before the
   point; D+nn from 10^16, D-nn when under 1 it needs more than 16 decimals; infinity 1.#INF.
   Returns its length. */
uint32_t dubl_fmt(double v, uint8_t *out);
/* A SINGLE so: 7 digits, E (flt_fmt). */
uint32_t dubl_fmtf(Sngl v, uint8_t *out);

/* The ops: vm/exec's handlers, its ip at the operands, its sp the data stack. */
void dubl_ldv(Vm *vm);  /* D.LD.V id ( -- d ) */
void dubl_stv(Vm *vm);  /* D.ST.V id ( d -- ) */
void dubl_ldk(Vm *vm);  /* D.LD.K q:8 ( -- d ) */
void dubl_lda1(Vm *vm); /* D.LD.A1 id ( i -- d ) */
void dubl_lda(Vm *vm);  /* D.LD.A id n ( i1..in -- d ) */
void dubl_sta1(Vm *vm); /* D.ST.A1 id ( i d -- ) */
void dubl_sta(Vm *vm);  /* D.ST.A id n ( i1..in d -- ) */
void dubl_add(Vm *vm);  /* D.ADD ( a b -- a+b ) */
void dubl_sub(Vm *vm);  /* D.SUB ( a b -- a-b ) */
void dubl_mul(Vm *vm);  /* D.MUL ( a b -- a*b ) */
void dubl_div(Vm *vm);  /* D.DIV ( a b -- a/b ); ?DIVISION BY ZERO */
void dubl_neg(Vm *vm);  /* D.NEG ( d -- -d ); +-0 kept */
void dubl_cmp(Vm *vm);  /* D.CMP mask ( a b -- i ) -1 when the relation holds */
void dubl_nz(Vm *vm);   /* D.NZ ( d -- i ) -1 unless +-0 */
void dubl_itod(Vm *vm); /* I.TOD ( i -- d ) */
void dubl_ftod(Vm *vm); /* F.TOD ( f -- d ) */
void dubl_tof(Vm *vm);  /* D.TOF ( d -- f ) rounded; infinity past the range, no error */
void dubl_toi(Vm *vm);  /* D.TOI ( d -- i ) a half to even; ?OVERFLOW past an INT */
void dubl_fn(Vm *vm);   /* D.FN k ( d -- d ) DF_*; ?ILLEGAL QUANTITY as fmth's, SQR under 0 NaN */
void dubl_pow(Vm *vm);  /* D.POW ( a b -- a^b ) as fmth_pow */
void dubl_print(Vm *vm); /* DPR ( d -- ) as PRINT, a space after */
void dubl_str(Vm *vm);  /* DSTR ( d -- s ) STR$ */
#endif
