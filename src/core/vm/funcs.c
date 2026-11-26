/* The functions' CALL routines: SGN .. CHR$ by token, LEFT$, RIGHT$, MID$, and QBasic's string,
 * bound and error functions.
 *
 * # How it works
 * SGN .. CHR$ share one handler, func_fn1, that picks a function of one value from a table by the
 * token (K_FN + token - TK_SGN). Each of the others has its own K handler (ops.def); nothing else
 * is called from outside. A string result is a temporary.
 *
 * # Example
 *   PRINT LEFT$("hello", 2)     he
 *
 * # asm structure (retired/asm32/vm/builtins.asm)
 * | asm | here | |
 * |---|---|---|
 * | sc_fn1 | func_fn1 (all of K_FN_SGN .. K_FN_CHRS) | MIRROR |
 * | sc_left, sc_right, sc_mid, sc_mid2 | func_left, func_right, func_mid, func_mid2 | MIRROR |
 * | sc_fix, sc_instr, sc_lbnd, sc_ubnd, sc_space, sc_strg | func_fix, func_instr, func_lbnd, func_ubnd, func_space, func_strg | MIRROR |
 * | sc_ltrim, sc_rtrim, sc_ucase, sc_lcase, sc_hex, sc_oct | func_ltrim, func_rtrim, func_ucase, func_lcase, func_hex, func_oct | MIRROR |
 * | sc_sfix, sc_mids, sc_vseg, sc_sadd, sc_err, sc_erl | func_sfix, func_mids, func_vseg, func_sadd, func_err, func_erl | MIRROR |
 * | sc_error, sc_dval | func_error, func_dval | MIRROR |
 * | fn_sgn, fn_int, fn_abs, fn_usr, fn_fre, fn_pos, fn_peek, fn_len, fn_asc, fn_chr, fn_fstr | f_sgn .. f_chr, rows of the fn1 table (with num's fmth_sqr .. rndm_rnd) | MIRROR |
 * | fn_left, fn_right, fn_mid, str_from | static | MIRROR |
 * | g_fixcw | fmth_fix (num) | MOVED |
 */
#include "vm/funcs.h"
#include "session.h"
#include "comp/declare.h"
#include "io/console.h"
#include "io/repl.h"
#include "mem/array.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "num/double.h"
#include "num/fpmath.h"
#include "num/random.h"
#include "qbrt/qbrt.h"

/* ---- functions of one value, by token ------------------------------------------------------- */

typedef uint32_t Fn1(Session *S, uint32_t);

static uint32_t f_sgn(Session *const S, const uint32_t a) { return (uint32_t)sngl_sgn(a); }
static uint32_t f_int(Session *const S, const uint32_t a) { return fmth_floor(a); }
static uint32_t f_abs(Session *const S, const uint32_t a) { return sngl_abs(a); }
static uint32_t f_usr(Session *const S, const uint32_t)
{
    repl_error(S, E_IQ);
} /* no routine is set */

/* FRE(n): the free bytes; FRE(s$) is func_fres. */
static uint32_t f_fre(Session *const S, const uint32_t)
{
    strg_gc(&S->strg);
    return fmth_from_i32((int32_t)allo_free(&S->allo));
}

static uint32_t f_pos(Session *const S, const uint32_t) { return S->cons.col + 1u; } /* from 1 */
static uint32_t f_sqr(Session *const S, const uint32_t a) { return fmth_sqr(&S->fault, a); }
static uint32_t f_rnd(Session *const S, const uint32_t a) { return rndm_rnd(&S->rndm, a); }
static uint32_t f_log(Session *const S, const uint32_t a) { return fmth_log(&S->fault, a); }
static uint32_t f_exp(Session *const S, const uint32_t a) { return fmth_exp(&S->fault, a); }
static uint32_t f_cos(Session *const S, const uint32_t a) { return fmth_cos(&S->fault, a); }
static uint32_t f_sin(Session *const S, const uint32_t a) { return fmth_sin(&S->fault, a); }
static uint32_t f_tan(Session *const S, const uint32_t a) { return fmth_tan(&S->fault, a); }
static uint32_t f_atn(Session *const S, const uint32_t a) { return fmth_atn(&S->fault, a); }
static uint32_t f_peek(Session *const S, const uint32_t a) { return rd8(a); }
static uint32_t f_len(Session *const S, const uint32_t a) { return strg_get(&S->strg, a).len; }

/* STR$: PRINT's text captured, its trailing space left out. */
static uint32_t f_str(Session *const S, const uint32_t a)
{
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    sngl_print(&S->cons, a);
    S->cons.capture &= ~CAP_SBUF;
    return strg_text(&S->strg, S->cons.sbuf, (uint16_t)(S->cons.sblen - 1));
}

/* ASC. Illegal function call: empty. */
static uint32_t f_asc(Session *const S, const uint32_t a)
{
    const Desc s = strg_get(&S->strg, a);

    if (!s.len)
        repl_error(S, E_IQ);

    return rd8(s.ptr);
}

static uint32_t f_chr(Session *const S, const uint32_t a)
{
    S->cons.sbuf[0] = (uint8_t)a;
    return strg_text(&S->strg, S->cons.sbuf, 1);
}

/* By K_FN_SGN .. K_FN_CHRS; VAL is K_DVAL. */
static Fn1 *const fns[] = {
    f_sgn, f_int, f_abs, f_usr, f_fre,  f_pos, f_sqr, f_rnd,   f_log, f_exp,
    f_cos, f_sin, f_tan, f_atn, f_peek, f_len, f_str, nullptr, f_asc, f_chr,
};
static_assert(countof(fns) == K_FN_CHRS - K_FN + 1);

/* A function of one value ( x -- y ): the CALL's k picks it. */
void func_fn1(Session *const S)
{
    const uint8_t k = S->vm.ip[-1];
    exec_pusha(&S->vm, fns[k - K_FN](S, exec_pop(&S->vm).u));
}

void func_fres(Session *const S)
{
    strg_get(&S->strg, exec_popa(&S->vm));
    strg_gc(&S->strg);
    exec_pusha(&S->vm, fmth_from_i32((int32_t)allo_string_room(&S->allo)));
}

/* ---- substrings ----------------------------------------------------------------------------- */

/* LEFT$ ( s n -- s ). */
void func_left(Session *const S)
{
    const uint16_t n = (uint16_t)exec_pop(&S->vm).u;
    exec_pusha(&S->vm, strg_sub(&S->strg, exec_popa(&S->vm), 0, n));
}

/* RIGHT$ ( s n -- s ). */
void func_right(Session *const S)
{
    const uint16_t n = (uint16_t)exec_pop(&S->vm).u;
    const Addr s = exec_popa(&S->vm);
    const uint16_t len = strg_len(s);
    exec_pusha(&S->vm, strg_sub(&S->strg, s, n > len ? 0 : len - n, n));
}

/* MID$ ( s i n -- s ), ( s i -- s ). Illegal function call: i 0. */
static void mid(Session *const S, const uint16_t n)
{
    const uint16_t i = (uint16_t)exec_pop(&S->vm).u;
    const Addr s = exec_popa(&S->vm);

    if (!i)
        repl_error(S, E_IQ);

    exec_pusha(&S->vm, strg_sub(&S->strg, s, i - 1, n));
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

    exec_pusha(&S->vm, strg_sub(&S->strg, exec_popa(&S->vm), k, SMAX));
}

/* RTRIM$ ( s -- s ): without the trailing spaces. */
void func_rtrim(Session *const S)
{
    const Desc s = strg_peek(S->vm.sp->a);
    uint16_t len = s.len;

    while (len && rd8(s.ptr + len - 1) == ' ')
        len--;

    exec_pusha(&S->vm, strg_sub(&S->strg, exec_popa(&S->vm), 0, len));
}

int32_t func_find(const Desc s, const Desc t, const int32_t from)
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

/* INSTR ( [n] s t count -- i ): where t is in s from n, 1 the first; 0 none. Illegal function
   call: n under 1. */
void func_instr(Session *const S)
{
    const uint32_t count = exec_pop(&S->vm).u;
    const Desc t = strg_get(&S->strg, exec_popa(&S->vm));
    const Desc s = strg_get(&S->strg, exec_popa(&S->vm));
    const int32_t from = count == 3 ? exec_popi(&S->vm) : 1;

    if (from < 1)
        repl_error(S, E_IQ);

    exec_pushi(&S->vm, func_find(s, t, from));
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

    const Addr r = strg_sub(&S->strg, exec_popa(&S->vm), 0, SMAX);
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

    const Addr chars = strg_alloc(&S->strg, (uint16_t)n);
    memset(at(chars), c, n);
    exec_pusha(&S->vm, strg_new(&S->strg, chars, (uint16_t)n));
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
        const Desc s = strg_get(&S->strg, x);

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
    const Addr r = strg_sub(&S->strg, exec_popa(&S->vm), 0, SMAX);
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
    exec_pusha(&S->vm, strg_text(&S->strg, S->cons.sbuf, (uint16_t)n));
}
void func_hex(Session *const S) { radix(S, 4); }
void func_oct(Session *const S) { radix(S, 3); }

/* ( s n -- s ): s as a STRING * n, cut or padded with spaces. */
void func_sfix(Session *const S)
{
    const uint16_t n = (uint16_t)exec_pop(&S->vm).u;
    const Addr chars = strg_alloc(&S->strg, n);
    const Desc s = strg_get(&S->strg, exec_popa(&S->vm));
    const uint16_t k = s.len < n ? s.len : n;
    memcpy(at(chars), at(s.ptr), k);
    memset(at(chars + k), ' ', n - k);
    exec_pusha(&S->vm, strg_new(&S->strg, chars, n));
}

/* ---- the rest ------------------------------------------------------------------------------- */

/* VAL ( s -- d ), of a copy with a 0 after it: a DOUBLE, as BC's. */
void func_dval(Session *const S)
{
    const uint8_t *p = strg_near(&S->strg, strg_get(&S->strg, S->vm.sp->a));
    S->vm.sp->d = dubl_in(&S->fault, &p, nullptr);
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

/* VARPTR$ ( a type -- s ): the runtime's, of QBasic's type byte. */
void func_varps(Session *const S)
{
    const uint8_t type = (uint8_t)exec_pop(&S->vm).u;
    const Addr chars = strg_alloc(&S->strg, QBRT_VARPTR);
    qbrt_varptr(S->vm.sp->a, type, (char *)at(chars));
    S->vm.sp->a = strg_new(&S->strg, chars, QBRT_VARPTR);
}

/* SADD ( Desc -- a ): its chars' address. */
void func_sadd(Session *const S) { S->vm.sp->a = strg_peek(S->vm.sp->a).ptr; }

/* FIX ( f -- f ): toward 0. */
void func_fix(Session *const S) { S->vm.sp->u = fmth_fix(S->vm.sp->u); }

/* LBOUND, UBOUND ( array [d] count -- i ): of dimension d, 1 the first; the array an operand
   (exec_arrayop). Subscript out of range: no dimension d, or ERASEd. */
static void bound(Session *const S, const bool upper)
{
    const uint32_t count = exec_pop(&S->vm).u;
    const uint32_t d = count == 2 ? exec_pop(&S->vm).u : 1;
    ArrView v;
    const ArrSlot *const s = exec_arrayop(&S->arry, &S->vm, &v);

    if (s && !s->hdr) {
        if (s->dyn) /* ERASEd: none */
            arry_bad_sub(&S->arry);

        /* none yet: the running line's file's OPTION BASE to 10 */
        exec_pushi(&S->vm, upper ? 10 : decl_base(&S->decl, comp_module(&S->comp, S->vm.line)));
        return;
    }

    if (d < 1 || d > v.dims)
        arry_bad_sub(&S->arry);

    const int32_t lower = arry_view_lbound(&v, d - 1);
    exec_pushi(&S->vm, upper ? lower + (int32_t)arry_view_count(&v, d - 1) - 1 : lower);
}
void func_lbnd(Session *const S) { bound(S, false); }
void func_ubnd(Session *const S) { bound(S, true); }
