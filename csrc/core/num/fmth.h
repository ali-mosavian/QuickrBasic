/* SINGLE's arithmetic, conversions and functions (float/fpu.asm). + - * / and the conversions
   are C; the functions are the x87's, through plat/pfpu.h. Past the range a result is infinity,
   under it a denormal, as BC's. Binary ops are a OP b, a the left operand (the asm's ARG, EDX),
   b the right (its FAC, EAX); they share one signature for vm/exec's g_bflt. */
#ifndef NUM_FMTH_H
#define NUM_FMTH_H
#include "sngl.h"

/* The x87 set up; with none, says so and exits (flt_finit). */
void fmth_init(void);
/* After an error: the x87's stack empty, its control word the default (ex_fclr). */
void fmth_clear(void);

static inline Sngl fmth_add(const Sngl a, const Sngl b) { return sngl_bits(sngl_val(a) + sngl_val(b)); }
static inline Sngl fmth_sub(const Sngl a, const Sngl b) { return sngl_bits(sngl_val(a) - sngl_val(b)); }
static inline Sngl fmth_mul(const Sngl a, const Sngl b) { return sngl_bits(sngl_val(a) * sngl_val(b)); }
/* a / b; ?DIVISION BY ZERO when b is 0 or -0. */
Sngl fmth_div(Sngl a, Sngl b);
/* a ^ b, at 64 bits; a whole power by squaring. ?ILLEGAL QUANTITY a negative base to a fraction,
   0 to a negative power; ?OVERFLOW past 2^32767.5, infinity below it. */
Sngl fmth_pow(Sngl a, Sngl b);

static inline Sngl fmth_from_i16(const int16_t v) { return sngl_bits((float)v); }
static inline Sngl fmth_from_u16(const uint16_t v) { return sngl_bits((float)v); }
static inline Sngl fmth_from_i32(const int32_t v) { return sngl_bits((float)v); }
/* a rounded to the nearest, a half to even; ?ILLEGAL QUANTITY out of the type's range. */
int16_t fmth_to_i16(Sngl a);
uint16_t fmth_to_u16(Sngl a);
int32_t fmth_to_i32(Sngl a);
/* INT: floor(a). */
Sngl fmth_floor(Sngl a);
/* FIX: a toward 0 (declared for vm/builtins' sc_fix, which does it inline). */
Sngl fmth_fix(Sngl a);

/* The builtins of vm/exec's g_fntab. */
Sngl fmth_sqr(Sngl a); /* ?ILLEGAL QUANTITY under 0 */
Sngl fmth_exp(Sngl a); /* ?OVERFLOW from 2^32767.5; infinity below it */
Sngl fmth_log(Sngl a); /* ?ILLEGAL QUANTITY at 0 or under */
Sngl fmth_sin(Sngl a); /* SIN COS TAN: ?ILLEGAL QUANTITY past 2^31 */
Sngl fmth_cos(Sngl a);
Sngl fmth_tan(Sngl a);
Sngl fmth_atn(Sngl a);
#endif
