/* The functions' CALL routines (src/vm/builtins.asm): SGN .. CHR$ by token, LEFT$, RIGHT$, MID$,
   and QBasic's string, bound and error functions. */
#include "vm/func.h"
#include "comp/decl.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/arry.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "num/dubl.h"
#include "num/fmth.h"
#include "num/rndm.h"

static uint32_t f_sgn(uint32_t a) { return (uint32_t)sngl_sgn(a); }
static uint32_t f_int(uint32_t a) { return fmth_floor(a); }
static uint32_t f_abs(uint32_t a) { return sngl_abs(a); }
static uint32_t f_usr(uint32_t a) { repl_error(E_IQ); } /* no routine is set */
/* FRE: the heap's free bytes. */
static uint32_t f_fre(uint32_t a)
{
    strg_gc();
    return fmth_from_i32((int32_t)(strg.low - arry.end));
}
static uint32_t f_pos(uint32_t a) { return cons.col; }
static uint32_t f_sqr(uint32_t a) { return fmth_sqr(a); }
static uint32_t f_rnd(uint32_t a) { return rndm_rnd(a); }
static uint32_t f_log(uint32_t a) { return fmth_log(a); }
static uint32_t f_exp(uint32_t a) { return fmth_exp(a); }
static uint32_t f_cos(uint32_t a) { return fmth_cos(a); }
static uint32_t f_sin(uint32_t a) { return fmth_sin(a); }
static uint32_t f_tan(uint32_t a) { return fmth_tan(a); }
static uint32_t f_atn(uint32_t a) { return fmth_atn(a); }
static uint32_t f_peek(uint32_t a) { return rd8(a); }
static uint32_t f_len(uint32_t a) { return strg_get(a).len; }
/* STR$: PRINT's text captured, its trailing space left out. */
static uint32_t f_str(uint32_t a)
{
    cons.capture = CAP_SBUF;
    cons.sblen = 0;
    sngl_print(a);
    cons.capture &= ~CAP_SBUF;
    return strg_buftmp((uint16_t)(cons.sblen - 1));
}
/* ASC. Illegal function call: empty. */
static uint32_t f_asc(uint32_t a)
{
    Desc s = strg_get(a);
    if (!s.len)
        repl_error(E_IQ);
    return rd8(s.ptr);
}
static uint32_t f_chr(uint32_t a)
{
    cons.sbuf[0] = (uint8_t)a;
    return strg_buftmp(1);
}

/* g_fntab: by K_FN_SGN .. K_FN_CHRS; VAL is K_DVAL. */
static uint32_t (*const fns[])(uint32_t) = {
    f_sgn, f_int, f_abs, f_usr, f_fre, f_pos, f_sqr, f_rnd, f_log, f_exp,
    f_cos, f_sin, f_tan, f_atn, f_peek, f_len, f_str, nullptr, f_asc, f_chr,
};
static_assert(countof(fns) == K_FN_CHRS - K_FN + 1);

/* A function of one value ( x -- y ): the CALL's k picks it. */
void func_fn1(void)
{
    uint8_t k = exec.ip[-1];
    exec_pusha(fns[k - K_FN](exec_pop().u));
}

/* LEFT$ ( s n -- s ). */
void func_left(void)
{
    uint16_t n = (uint16_t)exec_pop().u;
    exec_pusha(strg_sub(exec_popa(), 0, n));
}

/* RIGHT$ ( s n -- s ). */
void func_right(void)
{
    uint16_t n = (uint16_t)exec_pop().u;
    Addr s = exec_popa();
    uint16_t len = strg_len(s);
    exec_pusha(strg_sub(s, n > len ? 0 : len - n, n));
}

/* MID$ ( s i n -- s ), ( s i -- s ). Illegal function call: i 0. */
static void mid(uint16_t n)
{
    uint16_t i = (uint16_t)exec_pop().u;
    Addr s = exec_popa();
    if (!i)
        repl_error(E_IQ);
    exec_pusha(strg_sub(s, i - 1, n));
}
void func_mid(void) { mid((uint16_t)exec_pop().u); }
void func_mid2(void) { mid(SMAX); }

/* VAL ( s -- d ), of a copy with a 0 after it: a DOUBLE, as BC's. */
void func_dval(void)
{
    const uint8_t *p = strg_near(strg_get(exec.sp->a));
    exec.sp->d = dubl_in(&p, nullptr);
}

/* VARSEG ( a -- 0 ): flat, no segment. */
void func_vseg(void) { exec.sp->u = 0; }

void func_err(void) { exec_pushi(exec.err); }
void func_erl(void) { exec_pushi(exec.erl); }

/* ERROR ( n -- ): QBasic's error n. Illegal function call: n not 1..255. */
void func_error(void)
{
    uint32_t n = exec_pop().u;
    if (n - 1 > 254)
        repl_error(E_IQ);
    exec.uerr = (uint16_t)n;
    repl_error(E_USR);
}

/* SADD ( Desc -- a ): its chars' address. */
void func_sadd(void) { exec.sp->u = rd32(exec.sp->a + 2); }

/* FIX ( f -- f ): toward 0. */
void func_fix(void) { exec.sp->u = fmth_fix(exec.sp->u); }

/* INSTR ( [n] s t count -- i ): where t is in s from n, 1 the first; 0 none. Illegal function
   call: n under 1. */
void func_instr(void)
{
    uint32_t count = exec_pop().u;
    Desc t = strg_get(exec_popa());
    Desc s = strg_get(exec_popa());
    int32_t n = count == 3 ? exec_popi() : 1;
    if (n < 1)
        repl_error(E_IQ);
    int32_t r = 0;
    if (n <= s.len) {
        if (!t.len)
            r = n;
        else
            for (int32_t i = n - 1; i <= s.len - t.len; i++)
                if (!memcmp(at(s.ptr + i), at(t.ptr), t.len)) {
                    r = i + 1;
                    break;
                }
    }
    exec_pushi(r);
}

/* LBOUND, UBOUND ( id [d] count -- i ): of dimension d, 1 the first. Subscript out of range: no
   dimension d, or ERASEd. */
static void bound(bool upper)
{
    uint32_t count = exec_pop().u;
    uint32_t d = count == 2 ? exec_pop().u : 1;
    uint16_t id = (uint16_t)exec_pop().u;
    Addr h = symb_var(id)->vt;
    if (!h) {
        if (symb_varx(id)->dyn) /* ERASEd: none */
            arry_bad_sub();
        exec_pushi(upper ? 10 : decl.base); /* none yet: OPTION BASE to 10 */
        return;
    }
    const ArrayHdr *a = (const ArrayHdr *)at(h);
    if (d - 1 >= a->dims)
        arry_bad_sub();
    int32_t r = (int16_t)a->count[d - 1 + a->dims]; /* its lower bound */
    if (upper)
        r += a->count[d - 1] - 1;
    exec_pushi(r);
}
void func_lbnd(void) { bound(false); }
void func_ubnd(void) { bound(true); }

/* n of c, a new temporary. Illegal function call: n past SMAX. */
static void repeat(uint32_t n, uint8_t c)
{
    if (n > SMAX)
        repl_error(E_IQ);
    Addr chars = strg_alloc((uint16_t)n);
    memset(at(chars), c, n);
    exec_pusha(strg_new(chars, (uint16_t)n));
}

/* SPACE$ ( n -- s ). */
void func_space(void) { repeat(exec_pop().u, ' '); }

/* STRING$ ( n x flag -- s ): n of x's first char, flag 1, else of char x. Illegal function call:
   x empty, not a char, n out of range. */
void func_strg(void)
{
    uint32_t flag = exec_pop().u;
    uint32_t x = exec_pop().u;
    uint8_t c;
    if (flag) {
        Desc s = strg_get(x);
        if (!s.len)
            repl_error(E_IQ);
        c = rd8(s.ptr);
    } else {
        if (x > 255)
            repl_error(E_IQ);
        c = (uint8_t)x;
    }
    repeat(exec_pop().u, c);
}

/* LTRIM$, RTRIM$ ( s -- s ): without the leading, the trailing spaces. */
void func_ltrim(void)
{
    Addr s = exec.sp->a;
    uint16_t len = rd16(s), k = 0;
    Addr p = rd32(s + 2);
    while (k < len && rd8(p + k) == ' ')
        k++;
    exec_pusha(strg_sub(exec_popa(), k, SMAX));
}
void func_rtrim(void)
{
    Addr s = exec.sp->a;
    uint16_t len = rd16(s);
    Addr p = rd32(s + 2);
    while (len && rd8(p + len - 1) == ' ')
        len--;
    exec_pusha(strg_sub(exec_popa(), 0, len));
}

/* UCASE$, LCASE$ ( s -- s ): a copy, its letters from lo..hi turned. */
static void recase(uint8_t lo, uint8_t hi)
{
    Addr r = strg_sub(exec_popa(), 0, SMAX);
    uint8_t *p = at(rd32(r + 2));
    for (uint16_t n = rd16(r); n; n--, p++)
        if (*p >= lo && *p <= hi)
            *p ^= 0x20;
    exec_pusha(r);
}
void func_ucase(void) { recase('a', 'z'); }
void func_lcase(void) { recase('A', 'Z'); }

/* HEX$, OCT$ ( i -- s ): its bits in base 16, 8. */
static void radix(uint8_t bits)
{
    uint32_t v = exec_pop().u;
    uint8_t t[16];
    uint32_t n = 0;
    do {
        uint8_t d = v & ((1u << bits) - 1);
        t[15 - n++] = d < 10 ? '0' + d : 'A' + d - 10;
        v >>= bits;
    } while (v);
    memcpy(cons.sbuf, t + 16 - n, n);
    exec_pusha(strg_buftmp((uint16_t)n));
}
void func_hex(void) { radix(4); }
void func_oct(void) { radix(3); }

/* ( s n -- s ): s as a STRING * n, cut or padded with spaces. */
void func_sfix(void)
{
    uint16_t n = (uint16_t)exec_pop().u;
    Addr chars = strg_alloc(n);
    Desc s = strg_get(exec_popa());
    uint16_t k = s.len < n ? s.len : n;
    memcpy(at(chars), at(s.ptr), k);
    memset(at(chars + k), ' ', n - k);
    exec_pusha(strg_new(chars, n));
}

/* The MID$ statement's value ( s i n v -- s ): s with v over its chars from i, n at most, its
   length kept. Illegal function call: i under 1. */
void func_mids(void)
{
    Addr v = exec_popa();
    uint32_t n = exec_pop().u;
    int32_t i = exec_popi();
    if (i < 1)
        repl_error(E_IQ);
    Addr r = strg_sub(exec_popa(), 0, SMAX);
    int32_t room = rd16(r) - (i - 1);
    if (room > 0) {
        if (n > (uint32_t)room)
            n = (uint32_t)room;
        if (n > rd16(v))
            n = rd16(v);
        memcpy(at(rd32(r + 2) + i - 1), at(rd32(v + 2)), n);
    }
    exec_pusha(r);
}
