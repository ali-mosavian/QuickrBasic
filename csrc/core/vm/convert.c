/* Numbers as their bytes: MKI$ .. CVD, and the Microsoft Binary Format forms.
 *
 * # How it works
 * A cell's bytes are the value's, low first. mk turns the top cell's first n bytes into a string;
 * cv turns a string's first n bytes into a cell.
 *
 * MBF: the significand low first, its sign in the top bit of the byte under the exponent; the
 * exponent last, 0 for zero, biased 129 with the point before the hidden 1 (IEEE's + 2 single,
 * - 894 double).
 *
 * # Example
 *   MKI$(258)         is CHR$(2) + CHR$(1)
 *   CVI(MKI$(258))    is 258
 *
 * # asm structure (src/vm/bin.asm)
 * | asm | here | |
 * |---|---|---|
 * | sc_mki, sc_mkl, sc_mks, sc_mkd, sc_mksm, sc_mkdm | mkcv_mki, mkcv_mkl, mkcv_mks, mkcv_mkd, mkcv_mksm, mkcv_mkdm | MIRROR |
 * | sc_cvi, sc_cvl, sc_cvs, sc_cvd, sc_cvsm, sc_cvdm | mkcv_cvi, mkcv_cvl, mkcv_cvs, mkcv_cvd, mkcv_cvsm, mkcv_cvdm | MIRROR |
 * | bn_mk, bn_cv | mk, cv | MIRROR |
 */
#include "vm/convert.h"
#include "session.h"
#include "io/console.h"
#include "io/repl.h"
#include "mem/string.h"
#include "vm/system.h"

/* IEEE and Microsoft Binary Format fields. MBF: the exponent in the top byte, biased 129 (2 over
   a SINGLE's 127), the sign under it, then the significand. */
enum : uint32_t {
    F32_MANT = 0x7FFFFF,
    F32_EXP_SHIFT = 23,
    F32_EXP_MAX = 0xFF,
    MBF_EXP_SHIFT = 24,
    MBF_BIAS_OVER_F32 = 2,
};
enum : uint64_t {
    F64_MANT = 0x000FFFFFFFFFFFFF,
    MBF64_MANT = 0x007FFFFFFFFFFFFF, /* 55 bits */
    F64_CARRY = 0x0010000000000000,  /* a 52-bit significand rounded past its top */
};
enum { F64_EXP_SHIFT = 52, F64_EXP_MAX = 0x7FF, MBF64_EXP_SHIFT = 56, MBF64_SIGN_SHIFT = 55 };
enum { F64_BIAS_OVER_MBF = 1023 - 129, F64_EXTRA_BITS = 3 };

/* The top cell's first n bytes, a string, instead of it. */
static void mk(Session *const S, const uint32_t n)
{
    const Cell c = exec_pop(&S->vm);
    memcpy(S->cons.sbuf, &c, n);
    syst_ret(S, n);
}

/* MKI$ ( i -- s ): its low 2 bytes. Overflow outside -32768 to 32767. */
void mkcv_mki(Session *const S)
{
    if (S->vm.sp->i != (int16_t)S->vm.sp->i)
        repl_error(S, E_OV);

    mk(S, 2);
}
void mkcv_mkl(Session *const S) { mk(S, 4); }
void mkcv_mks(Session *const S) { mk(S, 4); }
void mkcv_mkd(Session *const S) { mk(S, 8); }

/* MKSMBF$ ( f -- s ). 0 and denormals: 0. Overflow past MBF's range. */
static uint32_t sngl_to_mbf(Session *const S, const uint32_t v)
{
    const uint32_t e = v >> F32_EXP_SHIFT & F32_EXP_MAX;

    if (!e)
        return 0;

    const uint32_t mbf_e = e + MBF_BIAS_OVER_F32;

    if (mbf_e > F32_EXP_MAX)
        repl_error(S, E_OV);

    return (v & F32_MANT) | (v >> 31) << F32_EXP_SHIFT | mbf_e << MBF_EXP_SHIFT;
}

void mkcv_mksm(Session *const S)
{
    S->vm.sp->u = sngl_to_mbf(S, S->vm.sp->u);
    mk(S, 4);
}

/* MKDMBF$ ( d -- s ): the significand's 52 bits as 55. Too small: 0. Overflow past MBF's range. */
static uint64_t dubl_to_mbf(Session *const S, const uint64_t v)
{
    const int32_t e = (int32_t)(v >> F64_EXP_SHIFT & F64_EXP_MAX) - F64_BIAS_OVER_MBF;

    if (e <= 0)
        return 0;
    if (e > (int32_t)F32_EXP_MAX)
        repl_error(S, E_OV);

    return (v << F64_EXTRA_BITS & MBF64_MANT) | (v >> 63) << MBF64_SIGN_SHIFT |
           (uint64_t)e << MBF64_EXP_SHIFT;
}

void mkcv_mkdm(Session *const S)
{
    S->vm.sp->raw = dubl_to_mbf(S, S->vm.sp->raw);
    mk(S, 8);
}

/* The string on top, its first n bytes in the cell instead. Illegal function call: shorter. */
static void cv(Session *const S, const uint32_t n)
{
    const Desc s = strg_get(S, S->vm.sp->a);

    if (s.len < n)
        repl_error(S, E_IQ);

    memcpy(&S->vm.sp->raw, at(s.ptr), 8);
}

/* CVI ( s -- i ); CVL; CVS ( s -- f ); CVD ( s -- d ). */
void mkcv_cvi(Session *const S)
{
    cv(S, 2);
    S->vm.sp->i = (int16_t)S->vm.sp->u;
}
void mkcv_cvl(Session *const S) { cv(S, 4); }
void mkcv_cvs(Session *const S) { cv(S, 4); }
void mkcv_cvd(Session *const S) { cv(S, 8); }

/* CVSMBF ( s -- f ). 0, and under the SINGLE's range: 0. */
static uint32_t mbf_to_sngl(const uint32_t v)
{
    const uint32_t e = v >> MBF_EXP_SHIFT;

    if (e <= MBF_BIAS_OVER_F32)
        return 0;

    return (v >> F32_EXP_SHIFT & 1) << 31 | (v & F32_MANT) |
           (e - MBF_BIAS_OVER_F32) << F32_EXP_SHIFT;
}

void mkcv_cvsm(Session *const S)
{
    cv(S, 4);
    S->vm.sp->u = mbf_to_sngl(S->vm.sp->u);
}

/* CVDMBF ( s -- d ): 55 bits rounded to 52. */
static uint64_t mbf_to_dubl(const uint64_t v)
{
    const uint32_t mbf_e = (uint32_t)(v >> MBF64_EXP_SHIFT);

    if (!mbf_e)
        return 0;

    const uint64_t sign = v >> MBF64_SIGN_SHIFT & 1;
    const uint64_t rounded = ((v & MBF64_MANT) + (1 << (F64_EXTRA_BITS - 1))) >> F64_EXTRA_BITS;
    const bool carry = rounded & F64_CARRY;
    const uint64_t e = mbf_e + F64_BIAS_OVER_MBF + carry;
    return (rounded & F64_MANT) | e << F64_EXP_SHIFT | sign << 63;
}

void mkcv_cvdm(Session *const S)
{
    cv(S, 8);
    S->vm.sp->raw = mbf_to_dubl(S->vm.sp->raw);
}
