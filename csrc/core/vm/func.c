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

/* ---- functions of one value, by token ------------------------------------------------------- */

typedef uint32_t Fn1(uint32_t);

static uint32_t f_sgn(const uint32_t a) { return (uint32_t)sngl_sgn(a); }
static uint32_t f_int(const uint32_t a) { return fmth_floor(a); }
static uint32_t f_abs(const uint32_t a) { return sngl_abs(a); }
static uint32_t f_usr(const uint32_t) { repl_error(E_IQ); } /* no routine is set */

/* FRE: the heap's free bytes. */
static uint32_t f_fre(const uint32_t)
{
    strg_gc();
    return fmth_from_i32((int32_t)(strg.low - arry.end));
}

static uint32_t f_pos(const uint32_t) { return cons.col; }
static uint32_t f_sqr(const uint32_t a) { return fmth_sqr(a); }
static uint32_t f_rnd(const uint32_t a) { return rndm_rnd(a); }
static uint32_t f_log(const uint32_t a) { return fmth_log(a); }
static uint32_t f_exp(const uint32_t a) { return fmth_exp(a); }
static uint32_t f_cos(const uint32_t a) { return fmth_cos(a); }
static uint32_t f_sin(const uint32_t a) { return fmth_sin(a); }
static uint32_t f_tan(const uint32_t a) { return fmth_tan(a); }
static uint32_t f_atn(const uint32_t a) { return fmth_atn(a); }
static uint32_t f_peek(const uint32_t a) { return rd8(a); }
static uint32_t f_len(const uint32_t a) { return strg_get(a).len; }

/* STR$: PRINT's text captured, its trailing space left out. */
static uint32_t f_str(const uint32_t a)
{
    cons.capture = CAP_SBUF;
    cons.sblen = 0;
    sngl_print(a);
    cons.capture &= ~CAP_SBUF;
    return strg_buftmp((uint16_t)(cons.sblen - 1));
}

/* ASC. Illegal function call: empty. */
static uint32_t f_asc(const uint32_t a)
{
    const Desc s = strg_get(a);
    if (!s.len)
        repl_error(E_IQ);
    return rd8(s.ptr);
}

static uint32_t f_chr(const uint32_t a)
{
    cons.sbuf[0] = (uint8_t)a;
    return strg_buftmp(1);
}

/* By K_FN_SGN .. K_FN_CHRS; VAL is K_DVAL. */
static Fn1 *const fns[] = {
    f_sgn, f_int, f_abs, f_usr, f_fre, f_pos, f_sqr, f_rnd, f_log, f_exp,
    f_cos, f_sin, f_tan, f_atn, f_peek, f_len, f_str, nullptr, f_asc, f_chr,
};
static_assert(countof(fns) == K_FN_CHRS - K_FN + 1);

/* A function of one value ( x -- y ): the CALL's k picks it. */
void func_fn1(Vm *const vm)
{
    const uint8_t k = vm->ip[-1];
    exec_pusha(vm, fns[k - K_FN](exec_pop(vm).u));
}

/* ---- substrings ----------------------------------------------------------------------------- */

/* LEFT$ ( s n -- s ). */
void func_left(Vm *const vm)
{
    const uint16_t n = (uint16_t)exec_pop(vm).u;
    exec_pusha(vm, strg_sub(exec_popa(vm), 0, n));
}

/* RIGHT$ ( s n -- s ). */
void func_right(Vm *const vm)
{
    const uint16_t n = (uint16_t)exec_pop(vm).u;
    const Addr s = exec_popa(vm);
    const uint16_t len = strg_len(s);
    exec_pusha(vm, strg_sub(s, n > len ? 0 : len - n, n));
}

/* MID$ ( s i n -- s ), ( s i -- s ). Illegal function call: i 0. */
static void mid(Vm *const vm, const uint16_t n)
{
    const uint16_t i = (uint16_t)exec_pop(vm).u;
    const Addr s = exec_popa(vm);
    if (!i)
        repl_error(E_IQ);
    exec_pusha(vm, strg_sub(s, i - 1, n));
}
void func_mid(Vm *const vm) { mid(vm, (uint16_t)exec_pop(vm).u); }
void func_mid2(Vm *const vm) { mid(vm, SMAX); }

/* LTRIM$ ( s -- s ): without the leading spaces. */
void func_ltrim(Vm *const vm)
{
    const Desc s = strg_peek(vm->sp->a);
    uint16_t k = 0;
    while (k < s.len && rd8(s.ptr + k) == ' ')
        k++;
    exec_pusha(vm, strg_sub(exec_popa(vm), k, SMAX));
}

/* RTRIM$ ( s -- s ): without the trailing spaces. */
void func_rtrim(Vm *const vm)
{
    const Desc s = strg_peek(vm->sp->a);
    uint16_t len = s.len;
    while (len && rd8(s.ptr + len - 1) == ' ')
        len--;
    exec_pusha(vm, strg_sub(exec_popa(vm), 0, len));
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

void func_instr(Vm *const vm)
{
    const uint32_t count = exec_pop(vm).u;
    const Desc t = strg_get(exec_popa(vm));
    const Desc s = strg_get(exec_popa(vm));
    const int32_t from = count == 3 ? exec_popi(vm) : 1;
    if (from < 1)
        repl_error(E_IQ);
    exec_pushi(vm, instr(s, t, from));
}

/* The MID$ statement's value ( s i n v -- s ): s with v over its chars from i, n at most, its
   length kept. Illegal function call: i under 1. */
void func_mids(Vm *const vm)
{
    const Desc v = strg_peek(exec_popa(vm));
    const uint32_t n = exec_pop(vm).u;
    const int32_t i = exec_popi(vm);
    if (i < 1)
        repl_error(E_IQ);
    const Addr r = strg_sub(exec_popa(vm), 0, SMAX);
    const Desc dst = strg_peek(r);
    const int32_t room = dst.len - (i - 1);
    if (room > 0) {
        const uint32_t k = n < (uint32_t)room ? n : (uint32_t)room;
        memcpy(at(dst.ptr + i - 1), at(v.ptr), k < v.len ? k : v.len);
    }
    exec_pusha(vm, r);
}

/* ---- new strings ---------------------------------------------------------------------------- */

/* n of c, a new temporary. Illegal function call: n past SMAX. */
static void repeat(Vm *const vm, const uint32_t n, const uint8_t c)
{
    if (n > SMAX)
        repl_error(E_IQ);
    const Addr chars = strg_alloc((uint16_t)n);
    memset(at(chars), c, n);
    exec_pusha(vm, strg_new(chars, (uint16_t)n));
}

/* SPACE$ ( n -- s ). */
void func_space(Vm *const vm) { repeat(vm, exec_pop(vm).u, ' '); }

/* STRING$ ( n x is_string -- s ): n of x's first char, or of char x. Illegal function call: x
   empty, not a char, n out of range. */
void func_strg(Vm *const vm)
{
    const bool is_string = exec_pop(vm).u;
    const uint32_t x = exec_pop(vm).u;
    uint8_t c;
    if (is_string) {
        const Desc s = strg_get(x);
        if (!s.len)
            repl_error(E_IQ);
        c = rd8(s.ptr);
    } else {
        if (x > UINT8_MAX)
            repl_error(E_IQ);
        c = (uint8_t)x;
    }
    repeat(vm, exec_pop(vm).u, c);
}

/* UCASE$, LCASE$ ( s -- s ): a copy, its letters from first..last turned. */
static void recase(Vm *const vm, const uint8_t first, const uint8_t last)
{
    const Addr r = strg_sub(exec_popa(vm), 0, SMAX);
    const Desc s = strg_peek(r);
    uint8_t *const p = at(s.ptr);
    for (uint16_t i = 0; i < s.len; i++)
        if (p[i] >= first && p[i] <= last)
            p[i] ^= 'a' - 'A';
    exec_pusha(vm, r);
}
void func_ucase(Vm *const vm) { recase(vm, 'a', 'z'); }
void func_lcase(Vm *const vm) { recase(vm, 'A', 'Z'); }

/* HEX$, OCT$ ( i -- s ): its bits in base 16, 8. */
static void radix(Vm *const vm, const uint8_t bits)
{
    uint32_t v = exec_pop(vm).u;
    uint8_t digits[32];
    const uint32_t size = sizeof digits;
    uint32_t n = 0;
    do {
        const uint8_t d = v & ((1u << bits) - 1);
        digits[size - ++n] = d < 10 ? '0' + d : 'A' + d - 10;
        v >>= bits;
    } while (v);
    memcpy(cons.sbuf, digits + size - n, n);
    exec_pusha(vm, strg_buftmp((uint16_t)n));
}
void func_hex(Vm *const vm) { radix(vm, 4); }
void func_oct(Vm *const vm) { radix(vm, 3); }

/* ( s n -- s ): s as a STRING * n, cut or padded with spaces. */
void func_sfix(Vm *const vm)
{
    const uint16_t n = (uint16_t)exec_pop(vm).u;
    const Addr chars = strg_alloc(n);
    const Desc s = strg_get(exec_popa(vm));
    const uint16_t k = s.len < n ? s.len : n;
    memcpy(at(chars), at(s.ptr), k);
    memset(at(chars + k), ' ', n - k);
    exec_pusha(vm, strg_new(chars, n));
}

/* ---- the rest ------------------------------------------------------------------------------- */

/* VAL ( s -- d ), of a copy with a 0 after it: a DOUBLE, as BC's. */
void func_dval(Vm *const vm)
{
    const uint8_t *p = strg_near(strg_get(vm->sp->a));
    vm->sp->d = dubl_in(&p, nullptr);
}

/* VARSEG ( a -- 0 ): flat, no segment. */
void func_vseg(Vm *const vm) { vm->sp->u = 0; }

void func_err(Vm *const vm) { exec_pushi(vm, vm->err.code); }
void func_erl(Vm *const vm) { exec_pushi(vm, vm->err.line); }

/* ERROR ( n -- ): QBasic's error n. Illegal function call: n not 1..255. */
void func_error(Vm *const vm)
{
    const uint32_t n = exec_pop(vm).u;
    if (n < 1 || n > UINT8_MAX)
        repl_error(E_IQ);
    vm->err.user = (uint16_t)n;
    repl_error(E_USR);
}

/* SADD ( Desc -- a ): its chars' address. */
void func_sadd(Vm *const vm) { vm->sp->a = strg_peek(vm->sp->a).ptr; }

/* FIX ( f -- f ): toward 0. */
void func_fix(Vm *const vm) { vm->sp->u = fmth_fix(vm->sp->u); }

/* LBOUND, UBOUND ( id [d] count -- i ): of dimension d, 1 the first. Subscript out of range: no
   dimension d, or ERASEd. */
static void bound(Vm *const vm, const bool upper)
{
    const uint32_t count = exec_pop(vm).u;
    const uint32_t d = count == 2 ? exec_pop(vm).u : 1;
    const uint16_t id = (uint16_t)exec_pop(vm).u;
    const Addr hdr = symb_var(id)->vt;
    if (!hdr) {
        if (symb_varx(id)->dyn) /* ERASEd: none */
            arry_bad_sub();
        exec_pushi(vm, upper ? 10 : decl.base); /* none yet: OPTION BASE to 10 */
        return;
    }
    const ArrayHdr *const a = (const ArrayHdr *)at(hdr);
    if (d < 1 || d > a->dims)
        arry_bad_sub();
    const int32_t lower = (int16_t)a->count[d - 1 + a->dims];
    exec_pushi(vm, upper ? lower + a->count[d - 1] - 1 : lower);
}
void func_lbnd(Vm *const vm) { bound(vm, false); }
void func_ubnd(Vm *const vm) { bound(vm, true); }
