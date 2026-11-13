/* SINGLE (float/flt.asm): compares, conversions, number text in. The arithmetic is fmth's.

   A FLOAT is an IEEE single kept packed, a Sngl, as the asm's EAX: in a Cell's low dword (`.u`),
   its high dword undefined. Every routine of the num group takes and returns a Sngl, never a C
   float: llrm keeps a float result at the x87's precision until it is stored, and a Sngl is
   stored, so each operation rounds once, as the asm's fstp. */
#ifndef NUM_SNGL_H
#define NUM_SNGL_H
#include "defs.h"

typedef uint32_t Sngl;

enum : Sngl {
    SNGL_ONE = 0x3F800000, /* 1 (ftr.asm's g_kone) */
    SNGL_PI = 0x40490FDB,  /* pi rounded (ftr.asm's flt_pi) */
    SNGL_SIGN = 0x80000000,
};

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
Sngl sngl_in(const uint8_t **p);
/* Prints a as PRINT: a space or -, its digits (dubl_fmtf), a space. */
void sngl_print(Sngl a);
#endif
