/* DOUBLE's interface. How it works: double.c. */
#ifndef NUM_DOUBLE_H
#define NUM_DOUBLE_H
#include "single.h"

enum { DUBL_TXT = 32 }; /* dubl_fmt's text at most */

/* dubl_in's kind of number by its spelling. */
enum : uint8_t { DK_ANY = 0, DK_DBL = 1, DK_SNGL = 2 };

/* D.FN's operand byte. SGN's result is an INT. */
enum : uint8_t {
    DF_ABS,
    DF_SGN,
    DF_INT,
    DF_FIX,
    DF_SQR,
    DF_SIN,
    DF_COS,
    DF_TAN,
    DF_ATN,
    DF_LOG,
    DF_EXP,
};

/* D.CMP's mask: the relations that give -1. */
enum : uint8_t { DC_GT = 1, DC_EQ = 2, DC_LT = 4 }; /* NaN against anything: DC_EQ, as fcomp */

/* Reads &H|&O digits, or [sign] digits [. digits] [E|D [sign] digits] [# | !] at *p, spaces
   between skipped (BC's VAL and INPUT); *p past it. *kind (may be nullptr): DK_DBL by D, # or
   past 7 digits (zeros after the point counted), DK_SNGL by !. ?OVERFLOW past the largest. */
double dubl_in(Session *S, const uint8_t **p, uint8_t *kind);

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
void dubl_ldv(Session *S);  /* D.LD.V id ( -- d ) */
void dubl_stv(Session *S);  /* D.ST.V id ( d -- ) */
void dubl_ldk(Session *S);  /* D.LD.K q:8 ( -- d ) */
void dubl_lda1(Session *S); /* D.LD.A1 id ( i -- d ) */
void dubl_lda(Session *S);  /* D.LD.A id n ( i1..in -- d ) */
void dubl_sta1(Session *S); /* D.ST.A1 id ( i d -- ) */
void dubl_sta(Session *S);  /* D.ST.A id n ( i1..in d -- ) */
void dubl_add(Session *S);  /* D.ADD ( a b -- a+b ) */
void dubl_sub(Session *S);  /* D.SUB ( a b -- a-b ) */
void dubl_mul(Session *S);  /* D.MUL ( a b -- a*b ) */
void dubl_div(Session *S);  /* D.DIV ( a b -- a/b ); ?DIVISION BY ZERO */
void dubl_neg(Session *S);  /* D.NEG ( d -- -d ); +-0 kept */
void dubl_cmp(Session *S);  /* D.CMP mask ( a b -- i ) -1 when the relation holds */
void dubl_nz(Session *S);   /* D.NZ ( d -- i ) -1 unless +-0 */
void dubl_itod(Session *S); /* I.TOD ( i -- d ) */
void dubl_ftod(Session *S); /* F.TOD ( f -- d ) */
void dubl_tof(Session *S);  /* D.TOF ( d -- f ) rounded; infinity past the range, no error */
void dubl_toi(Session *S);  /* D.TOI ( d -- i ) a half to even; ?OVERFLOW past an INT */
void dubl_fn(Session *S); /* D.FN k ( d -- d ) DF_*; ?ILLEGAL QUANTITY as fmth's, SQR under 0 NaN */
void dubl_pow(Session *S);   /* D.POW ( a b -- a^b ) as fmth_pow */
void dubl_print(Session *S); /* DPR ( d -- ) as PRINT, a space after */
void dubl_str(Session *S);   /* DSTR ( d -- s ) STR$ */
#endif
