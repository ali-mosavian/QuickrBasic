/* SINGLE's interface. How it works: single.c. */
#ifndef NUM_SINGLE_H
#define NUM_SINGLE_H
#include "defs.h"
#include "fault.h"

typedef struct Cons Cons;

typedef uint32_t Sngl;

static inline float sngl_val(const Sngl a) { return (Cell){.u = a}.f; }

/* f rounded to a SINGLE: the store rounds an x87 result once. */
static inline Sngl sngl_bits(const float f) { return (Cell){.f = f}.u; }

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
/* a's place in the order of SINGLEs: sign and magnitude as a signed integer. */
static inline int32_t sngl_key(const Sngl a)
{
    const int32_t k = (int32_t)(a & ~SNGL_SIGN);
    return a & SNGL_SIGN ? -k : k;
}

/* a against b: 1 above, 0 equal (+0 and -0 too), -1 below; by the bits' order. */
static inline int sngl_cmp(const Sngl a, const Sngl b)
{
    const int32_t ka = sngl_key(a);
    const int32_t kb = sngl_key(b);
    return ka == kb ? 0 : ka > kb ? 1 : -1;
}
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
Sngl sngl_in(Fault *f, const uint8_t **p);
/* Prints a as PRINT: a space or -, its digits (dubl_fmtf), a space. */
void sngl_print(Cons *co, Sngl a);
#endif
