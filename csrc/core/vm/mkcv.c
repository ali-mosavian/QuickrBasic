/* Numbers as their bytes (src/vm/bin.asm): MKI$ .. CVD and the MBF forms. A cell's bytes are the
   value's, low first. */
#include "vm/mkcv.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/strg.h"
#include "vm/syst.h"

/* bn_mk: the top cell's first n bytes, a string, instead of it. */
static void mk(uint32_t n)
{
    Cell c = exec_pop();
    memcpy(cons.sbuf, &c, n);
    syst_ret(n);
}

/* MKI$ ( i -- s ): its low 2 bytes. Overflow outside -32768 to 32767. */
void mkcv_mki(void)
{
    if (exec.sp->i != (int16_t)exec.sp->i)
        repl_error(E_OV);
    mk(2);
}
void mkcv_mkl(void) { mk(4); }
void mkcv_mks(void) { mk(4); }
void mkcv_mkd(void) { mk(8); }

/* MKSMBF$ ( f -- s ). Overflow past MBF's range. */
void mkcv_mksm(void)
{
    uint32_t v = exec.sp->u;
    uint32_t e = v >> 23 & 0xFF;
    uint32_t m = 0;
    if (e) { /* 0 and denormals: 0 */
        e += 2;
        if (e > 0xFF)
            repl_error(E_OV);
        m = (v & 0x7FFFFF) | (v >> 31) << 23 | e << 24;
    }
    exec.sp->u = m;
    mk(4);
}

/* MKDMBF$ ( d -- s ): the significand's 52 bits as 55. */
void mkcv_mkdm(void)
{
    uint64_t v = exec.sp->raw;
    int32_t e = (int32_t)(v >> 52 & 0x7FF);
    uint64_t m = 0;
    if (e && (e -= 1023 - 129) > 0) { /* too small: 0 */
        if (e > 0xFF)
            repl_error(E_OV);
        uint32_t hi = (uint32_t)(v >> 32);
        m = (v << 3) & 0x007FFFFFFFFFFFFF;
        m |= (uint64_t)(hi >> 31) << 55 | (uint64_t)e << 56;
    }
    exec.sp->raw = m;
    mk(8);
}

/* bn_cv: the string at the top, its first n bytes in the cell instead. Illegal function call:
   shorter. */
static void cv(uint32_t n)
{
    Desc s = strg_get(exec.sp->a);
    if (s.len < n)
        repl_error(E_IQ);
    memcpy(&exec.sp->raw, at(s.ptr), 8);
}

/* CVI ( s -- i ); CVL; CVS ( s -- f ); CVD ( s -- d ). */
void mkcv_cvi(void)
{
    cv(2);
    exec.sp->i = (int16_t)exec.sp->u;
}
void mkcv_cvl(void) { cv(4); }
void mkcv_cvs(void) { cv(4); }
void mkcv_cvd(void) { cv(8); }

/* CVSMBF ( s -- f ). */
void mkcv_cvsm(void)
{
    cv(4);
    uint32_t v = exec.sp->u;
    uint32_t e = v >> 24;
    exec.sp->u = e <= 2 ? 0 /* 0, and under the single's range */
                        : (v >> 23 & 1) << 31 | (v & 0x7FFFFF) | (e - 2) << 23;
}

/* CVDMBF ( s -- d ): 55 bits rounded to 52. */
void mkcv_cvdm(void)
{
    cv(8);
    uint64_t v = exec.sp->raw;
    uint32_t e = (uint32_t)(v >> 56);
    uint64_t r = 0;
    if (e) {
        e += 1023 - 129;
        uint64_t sign = v >> 55 & 1;
        uint64_t m = ((v & 0x007FFFFFFFFFFFFF) + 4) >> 3; /* round the 3 bits away */
        if (m & 0x0010000000000000) { /* rounded up past the top */
            m &= 0x000FFFFFFFFFFFFF;
            e++;
        }
        r = m | (uint64_t)e << 52 | sign << 63;
    }
    exec.sp->raw = r;
}
