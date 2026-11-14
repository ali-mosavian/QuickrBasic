/* The functions' CALL routines (src/vm/builtins.asm): SGN .. CHR$ by token, LEFT$, RIGHT$, MID$,
   and QBasic's string, bound and error functions. */
#include "vm/func.h"
#include "session.h"
#include "comp/decl.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/arry.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "num/dubl.h"
#include "num/fmth.h"
#include "num/rndm.h"

/* ---- functions of one value, by token ------------------------------------------------------- */

typedef uint32_t Fn1(Session *S, uint32_t);

static uint32_t f_sgn(Session *const S, const uint32_t a) { return (uint32_t)sngl_sgn(a); }
static uint32_t f_int(Session *const S, const uint32_t a) { return fmth_floor(a); }
static uint32_t f_abs(Session *const S, const uint32_t a) { return sngl_abs(a); }
static uint32_t f_usr(Session *const S, const uint32_t) { repl_error(S, E_IQ); } /* no routine is set */

/* FRE: the heap's free bytes. */
static uint32_t f_fre(Session *const S, const uint32_t)
{
    strg_gc(S);
    return fmth_from_i32((int32_t)(S->strg.low - S->arry.end));
}

static uint32_t f_pos(Session *const S, const uint32_t) { return S->cons.col; }
static uint32_t f_sqr(Session *const S, const uint32_t a) { return fmth_sqr(S, a); }
static uint32_t f_rnd(Session *const S, const uint32_t a) { return rndm_rnd(S, a); }
static uint32_t f_log(Session *const S, const uint32_t a) { return fmth_log(S, a); }
static uint32_t f_exp(Session *const S, const uint32_t a) { return fmth_exp(S, a); }
static uint32_t f_cos(Session *const S, const uint32_t a) { return fmth_cos(S, a); }
static uint32_t f_sin(Session *const S, const uint32_t a) { return fmth_sin(S, a); }
static uint32_t f_tan(Session *const S, const uint32_t a) { return fmth_tan(S, a); }
static uint32_t f_atn(Session *const S, const uint32_t a) { return fmth_atn(S, a); }
static uint32_t f_peek(Session *const S, const uint32_t a) { return rd8(a); }
static uint32_t f_len(Session *const S, const uint32_t a) { return strg_get(S, a).len; }

/* STR$: PRINT's text captured, its trailing space left out. */
static uint32_t f_str(Session *const S, const uint32_t a)
{
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    sngl_print(S, a);
    S->cons.capture &= ~CAP_SBUF;
    return strg_buftmp(S, (uint16_t)(S->cons.sblen - 1));
}

/* ASC. Illegal function call: empty. */
static uint32_t f_asc(Session *const S, const uint32_t a)
{
    const Desc s = strg_get(S, a);
    if (!s.len)
        repl_error(S, E_IQ);
    return rd8(s.ptr);
}

static uint32_t f_chr(Session *const S, const uint32_t a)
{
    S->cons.sbuf[0] = (uint8_t)a;
    return strg_buftmp(S, 1);
}

/* By K_FN_SGN .. K_FN_CHRS; VAL is K_DVAL. */
static Fn1 *const fns[] = {
    f_sgn, f_int, f_abs, f_usr, f_fre, f_pos, f_sqr, f_rnd, f_log, f_exp,
    f_cos, f_sin, f_tan, f_atn, f_peek, f_len, f_str, nullptr, f_asc, f_chr,
};
static_assert(countof(fns) == K_FN_CHRS - K_FN + 1);

/* A function of one value ( x -- y ): the CALL's k picks it. */
void func_fn1(Session *const S)
{
    const uint8_t k = S->vm.ip[-1];
    exec_pusha(&S->vm, fns[k - K_FN](S, exec_pop(&S->vm).u));
}

/* ---- substrings ----------------------------------------------------------------------------- */

/* LEFT$ ( s n -- s ). */
void func_left(Session *const S)
{
    const uint16_t n = (uint16_t)exec_pop(&S->vm).u;
    exec_pusha(&S->vm, strg_sub(S, exec_popa(&S->vm), 0, n));
}

/* RIGHT$ ( s n -- s ). */
void func_right(Session *const S)
{
    const uint16_t n = (uint16_t)exec_pop(&S->vm).u;
    const Addr s = exec_popa(&S->vm);
    const uint16_t len = strg_len(s);
    exec_pusha(&S->vm, strg_sub(S, s, n > len ? 0 : len - n, n));
}

/* MID$ ( s i n -- s ), ( s i -- s ). Illegal function call: i 0. */
static void mid(Session *const S, const uint16_t n)
{
    const uint16_t i = (uint16_t)exec_pop(&S->vm).u;
    const Addr s = exec_popa(&S->vm);
    if (!i)
        repl_error(S, E_IQ);
    exec_pusha(&S->vm, strg_sub(S, s, i - 1, n));
}
void func_mid(Session *const S) { mid(S, (uint16_t)exec_pop(&S->vm).u); }
void func_mid2(Session *const S) { mid(S, SMAX); }

/* LTRIM$ ( s -- s ): without the leading spaces. */
void func_ltrim(Session *const S)
{
    const Desc s = strg_peek(S->vm.sp->a);
    uint16_t k = 0;
    while (k < s.len && rd8(s.ptr + k) == ' ')
        k++;
    exec_pusha(&S->vm, strg_sub(S, exec_popa(&S->vm), k, SMAX));
}

/* RTRIM$ ( s -- s ): without the trailing spaces. */
void func_rtrim(Session *const S)
{
    const Desc s = strg_peek(S->vm.sp->a);
    uint16_t len = s.len;
    while (len && rd8(s.ptr + len - 1) == ' ')
        len--;
    exec_pusha(&S->vm, strg_sub(S, exec_popa(&S->vm), 0, len));
}

/* INSTR ( [n] s t count -- i ): where t is in s from n, 1 the first; 0 none. Illegal function
   call: n under 1. */
static int32_t instr(const Desc s, const Desc t, const int32_t from)
{
    if (from > s.len)
        return 0;
    if (!t.len)
        return from;
    for (int32_t i = from - 1; i <= s.len - t.len; i++)
        if (!memcmp(at(s.ptr + i), at(t.ptr), t.len))
            return i + 1;
    return 0;
}

void func_instr(Session *const S)
{
    const uint32_t count = exec_pop(&S->vm).u;
    const Desc t = strg_get(S, exec_popa(&S->vm));
    const Desc s = strg_get(S, exec_popa(&S->vm));
    const int32_t from = count == 3 ? exec_popi(&S->vm) : 1;
    if (from < 1)
        repl_error(S, E_IQ);
    exec_pushi(&S->vm, instr(s, t, from));
}

/* The MID$ statement's value ( s i n v -- s ): s with v over its chars from i, n at most, its
   length kept. Illegal function call: i under 1. */
void func_mids(Session *const S)
{
    const Desc v = strg_peek(exec_popa(&S->vm));
    const uint32_t n = exec_pop(&S->vm).u;
    const int32_t i = exec_popi(&S->vm);
    if (i < 1)
        repl_error(S, E_IQ);
    const Addr r = strg_sub(S, exec_popa(&S->vm), 0, SMAX);
    const Desc dst = strg_peek(r);
    const int32_t room = dst.len - (i - 1);
    if (room > 0) {
        const uint32_t k = n < (uint32_t)room ? n : (uint32_t)room;
        memcpy(at(dst.ptr + i - 1), at(v.ptr), k < v.len ? k : v.len);
    }
    exec_pusha(&S->vm, r);
}

/* ---- new strings ---------------------------------------------------------------------------- */

/* n of c, a new temporary. Illegal function call: n past SMAX. */
static void repeat(Session *const S, const uint32_t n, const uint8_t c)
{
    if (n > SMAX)
        repl_error(S, E_IQ);
    const Addr chars = strg_alloc(S, (uint16_t)n);
    memset(at(chars), c, n);
    exec_pusha(&S->vm, strg_new(S, chars, (uint16_t)n));
}

/* SPACE$ ( n -- s ). */
void func_space(Session *const S) { repeat(S, exec_pop(&S->vm).u, ' '); }

/* STRING$ ( n x is_string -- s ): n of x's first char, or of char x. Illegal function call: x
   empty, not a char, n out of range. */
void func_strg(Session *const S)
{
    const bool is_string = exec_pop(&S->vm).u;
    const uint32_t x = exec_pop(&S->vm).u;
    uint8_t c;
    if (is_string) {
        const Desc s = strg_get(S, x);
        if (!s.len)
            repl_error(S, E_IQ);
        c = rd8(s.ptr);
    } else {
        if (x > UINT8_MAX)
            repl_error(S, E_IQ);
        c = (uint8_t)x;
    }
    repeat(S, exec_pop(&S->vm).u, c);
}

/* UCASE$, LCASE$ ( s -- s ): a copy, its letters from first..last turned. */
static void recase(Session *const S, const uint8_t first, const uint8_t last)
{
    const Addr r = strg_sub(S, exec_popa(&S->vm), 0, SMAX);
    const Desc s = strg_peek(r);
    uint8_t *const p = at(s.ptr);
    for (uint16_t i = 0; i < s.len; i++)
        if (p[i] >= first && p[i] <= last)
            p[i] ^= 'a' - 'A';
    exec_pusha(&S->vm, r);
}
void func_ucase(Session *const S) { recase(S, 'a', 'z'); }
void func_lcase(Session *const S) { recase(S, 'A', 'Z'); }

/* HEX$, OCT$ ( i -- s ): its bits in base 16, 8. */
static void radix(Session *const S, const uint8_t bits)
{
    uint32_t v = exec_pop(&S->vm).u;
    uint8_t digits[32];
    const uint32_t size = sizeof digits;
    uint32_t n = 0;
    do {
        const uint8_t d = v & ((1u << bits) - 1);
        digits[size - ++n] = d < 10 ? '0' + d : 'A' + d - 10;
        v >>= bits;
    } while (v);
    memcpy(S->cons.sbuf, digits + size - n, n);
    exec_pusha(&S->vm, strg_buftmp(S, (uint16_t)n));
}
void func_hex(Session *const S) { radix(S, 4); }
void func_oct(Session *const S) { radix(S, 3); }

/* ( s n -- s ): s as a STRING * n, cut or padded with spaces. */
void func_sfix(Session *const S)
{
    const uint16_t n = (uint16_t)exec_pop(&S->vm).u;
    const Addr chars = strg_alloc(S, n);
    const Desc s = strg_get(S, exec_popa(&S->vm));
    const uint16_t k = s.len < n ? s.len : n;
    memcpy(at(chars), at(s.ptr), k);
    memset(at(chars + k), ' ', n - k);
    exec_pusha(&S->vm, strg_new(S, chars, n));
}

/* ---- the rest ------------------------------------------------------------------------------- */

/* VAL ( s -- d ), of a copy with a 0 after it: a DOUBLE, as BC's. */
void func_dval(Session *const S)
{
    const uint8_t *p = strg_near(S, strg_get(S, S->vm.sp->a));
    S->vm.sp->d = dubl_in(S, &p, nullptr);
}

/* VARSEG ( a -- 0 ): flat, no segment. */
void func_vseg(Session *const S) { S->vm.sp->u = 0; }

void func_err(Session *const S) { exec_pushi(&S->vm, S->vm.err.code); }
void func_erl(Session *const S) { exec_pushi(&S->vm, S->vm.err.line); }

/* ERROR ( n -- ): QBasic's error n. Illegal function call: n not 1..255. */
void func_error(Session *const S)
{
    const uint32_t n = exec_pop(&S->vm).u;
    if (n < 1 || n > UINT8_MAX)
        repl_error(S, E_IQ);
    S->vm.err.user = (uint16_t)n;
    repl_error(S, E_USR);
}

/* SADD ( Desc -- a ): its chars' address. */
void func_sadd(Session *const S) { S->vm.sp->a = strg_peek(S->vm.sp->a).ptr; }

/* FIX ( f -- f ): toward 0. */
void func_fix(Session *const S) { S->vm.sp->u = fmth_fix(S->vm.sp->u); }

/* LBOUND, UBOUND ( id [d] count -- i ): of dimension d, 1 the first. Subscript out of range: no
   dimension d, or ERASEd. */
static void bound(Session *const S, const bool upper)
{
    const uint32_t count = exec_pop(&S->vm).u;
    const uint32_t d = count == 2 ? exec_pop(&S->vm).u : 1;
    const uint16_t id = (uint16_t)exec_pop(&S->vm).u;
    const Addr hdr = symb_var(&S->symb, id)->vt;
    if (!hdr) {
        if (symb_varx(&S->symb, id)->dyn) /* ERASEd: none */
            arry_bad_sub(S);
        exec_pushi(&S->vm, upper ? 10 : S->decl.base); /* none yet: OPTION BASE to 10 */
        return;
    }
    const ArrayHdr *const a = (const ArrayHdr *)at(hdr);
    if (d < 1 || d > a->dims)
        arry_bad_sub(S);
    const int32_t lower = (int16_t)a->count[d - 1 + a->dims];
    exec_pushi(&S->vm, upper ? lower + a->count[d - 1] - 1 : lower);
}
void func_lbnd(Session *const S) { bound(S, false); }
void func_ubnd(Session *const S) { bound(S, true); }
