/* SINGLE (float/flt.asm): compares, conversions, number text in. The arithmetic is fmth's.

   A FLOAT is an IEEE single kept packed, a Sngl, as the asm's EAX: in a Cell's low dword (`.u`),
   its high dword undefined. Every routine of the num group takes and returns a Sngl, never a C
   float: llrm keeps a float result at the x87's precision until it is stored, and a Sngl is
   stored, so each operation rounds once, as the asm's fstp. */
#ifndef NUM_SINGLE_H
#define NUM_SINGLE_H
#include "defs.h"

typedef uint32_t Sngl;

static inline float sngl_val(const Sngl a)
{
    return (Cell){.u = a}.f;
}

/* f rounded to a SINGLE: the store rounds an x87 result once. */
static inline Sngl sngl_bits(const float f)
{
    return (Cell){.f = f}.u;
}

enum : Sngl {
    SNGL_ONE = 0x3F800000, /* 1 (ftr.asm's g_kone) */
    SNGL_PI = 0x40490FDB,  /* pi rounded (ftr.asm's flt_pi) */
    SNGL_HUNDRED = 0x42C80000,
    SNGL_SIGN = 0x80000000,
    SNGL_MANT = 0x7FFFFF,
    SNGL_IMPLICIT = 0x800000, /* the significand's leading 1 */
};
enum { SNGL_EXP_SHIFT = 23, SNGL_EXP_MASK = 0xFF };

/* |a|. */
Sngl sngl_abs(Sngl a);
/* a against b: 1 above, 0 equal (+0 and -0 too), -1 below; by the bits' order. */
int sngl_cmp(Sngl a, Sngl b);
/* a's sign: 1, 0 or -1. */
int sngl_sgn(Sngl a);
/* mag (not 0, under 2^24) with neg's sign, exactly. */
Sngl sngl_norm(uint32_t mag, bool neg);
/* a is whole and under 32768 in size: *out its value. */
bool sngl_w16(Sngl a, int16_t *out);
/* Steps *p past spaces and a sign, '+' '-' or their tokens TK_PLUS TK_MINUS (comp/dfa.inc);
   true for a minus. */
bool sngl_skipsign(const uint8_t **p);
/* *p after '&': H or O and its digits into *val, *p past them; false neither, *p kept. */
bool sngl_hexoct(const uint8_t **p, uint32_t *val);
/* A DOUBLE rounded to a SINGLE (flt_dv). */
Sngl sngl_from_dbl(double d);
/* dubl_in's number at *p as a SINGLE, *p past it. */
Sngl sngl_in(Session *S, const uint8_t **p);
/* Prints a as PRINT: a space or -, its digits (dubl_fmtf), a space. */
void sngl_print(Session *S, Sngl a);
#endif
