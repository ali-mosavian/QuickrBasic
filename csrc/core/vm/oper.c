/* The simple ops (src/vm/bc.asm): loads, stores, INT and FLOAT operators, compares, IF,
   conversions, DROP. An INT op works on the top cell's low dword. */
#include "vm/oper.h"
#include "io/repl.h"
#include "mem/symb.h"
#include "num/fmth.h"
#include "vm/flow.h"

const uint8_t oper_relbit[3] = {OPER_GT, OPER_EQ, OPER_LT};

/* The variable at exec.ip's own cell. */
static Addr var(void) { return symb_cell(exec_u16()); }

/* The right operand: src 0 popped, 1 a variable, 2 the U8 (INT); 3 an INT variable as a FLOAT,
   4 the U8 as a FLOAT, 5 the F32. */
static uint32_t operand(int src)
{
    switch (src) {
    case 0: return exec_pop().u;
    case 1: return rd32(var());
    case 2: return exec_u8();
    case 3: return fmth_from_i32((int32_t)rd32(var()));
    case 4: return fmth_from_i32(exec_u8());
    default: return exec_u32();
    }
}

/* X.LD.V id: the variable's cell. */
void oper_i_ld_v(void) { exec_pusha(rd32(var())); }
/* I.LD.I16 w: the word, unsigned. */
void oper_i_ld_i16(void) { exec_pusha(exec_u16()); }
void oper_i_ld_i32(void) { exec_pusha(exec_u32()); }
void oper_i_ld_u8(void) { exec_pusha(exec_u8()); }
void oper_f_ld_f32(void) { exec_pusha(exec_u32()); }
void oper_f_ld_u8(void) { exec_pusha(operand(4)); }
/* F.LD.VI id: the INT variable as a FLOAT. */
void oper_f_ld_vi(void) { exec_pusha(operand(3)); }
void oper_f_ld_pi(void) { exec_pusha(SNGL_PI); }
/* S.LD.V id: the string variable's Desc. */
void oper_s_ld_v(void) { exec_pusha(var()); }
/* X.ST.V id ( x -- ). */
void oper_i_st_v(void)
{
    Addr c = var();
    wr32(c, exec_pop().u);
}
void oper_f_st_v(void) { oper_i_st_v(); }

/* INT operators on the top cell: k 0 + 1 - 2 * 3 / 4 AND 5 OR. Overflow. */
static void ibin(int src, int k)
{
    int32_t b = (int32_t)operand(src);
    Cell *t = exec.sp;
    int32_t r, rem;
    switch (k) {
    case 0:
        if (__builtin_add_overflow(t->i, b, &r))
            repl_error(E_OV);
        break;
    case 1:
        if (__builtin_sub_overflow(t->i, b, &r))
            repl_error(E_OV);
        break;
    case 2:
        if (__builtin_mul_overflow(t->i, b, &r))
            repl_error(E_OV);
        break;
    case 3: r = exec_idiv(t->i, b, &rem); break;
    case 4: r = t->i & b; break;
    default: r = t->i | b; break;
    }
    t->i = r;
}

void oper_i_add(void) { ibin(0, 0); }
void oper_i_sub(void) { ibin(0, 1); }
void oper_i_mul(void) { ibin(0, 2); }
void oper_i_div(void) { ibin(0, 3); }
void oper_i_and(void) { ibin(0, 4); }
void oper_i_or(void) { ibin(0, 5); }
void oper_i_add_v(void) { ibin(1, 0); }
void oper_i_sub_v(void) { ibin(1, 1); }
void oper_i_mul_v(void) { ibin(1, 2); }
void oper_i_div_v(void) { ibin(1, 3); }
void oper_i_and_v(void) { ibin(1, 4); }
void oper_i_or_v(void) { ibin(1, 5); }
void oper_i_add_u8(void) { ibin(2, 0); }
void oper_i_sub_u8(void) { ibin(2, 1); }
void oper_i_mul_u8(void) { ibin(2, 2); }
void oper_i_div_u8(void) { ibin(2, 3); }
void oper_i_and_u8(void) { ibin(2, 4); }
void oper_i_or_u8(void) { ibin(2, 5); }

/* I.MOD ( a b -- a MOD b ): the sign of a. */
void oper_i_mod(void)
{
    int32_t b = exec_popi();
    int32_t rem = 0;
    if (b != -1)
        exec_idiv(exec.sp->i, b, &rem);
    exec.sp->i = rem;
}

void oper_i_xor(void)
{
    int32_t b = exec_popi();
    exec.sp->i ^= b;
}
void oper_i_eqv(void)
{
    int32_t b = exec_popi();
    exec.sp->i = ~(exec.sp->i ^ b);
}
void oper_i_imp(void)
{
    int32_t b = exec_popi();
    exec.sp->i = ~exec.sp->i | b;
}

/* I.NEG ( i -- -i ). Overflow. */
void oper_i_neg(void)
{
    if (exec.sp->i == INT32_MIN)
        repl_error(E_OV);
    exec.sp->i = -exec.sp->i;
}
void oper_i_not(void) { exec.sp->i = ~exec.sp->i; }

/* FLOAT operators: the right operand by src (as operand's 0, 1, 3, 4, 5), the left popped. */
static Sngl (*const fops[4])(Sngl, Sngl) = {fmth_add, fmth_sub, fmth_mul, fmth_div};
static void fbin(int src, int k)
{
    Sngl b = operand(src);
    Sngl a = exec_pop().u;
    exec_pusha(fops[k](a, b));
}

void oper_f_add(void) { fbin(0, 0); }
void oper_f_sub(void) { fbin(0, 1); }
void oper_f_mul(void) { fbin(0, 2); }
void oper_f_div(void) { fbin(0, 3); }
void oper_f_add_v(void) { fbin(1, 0); }
void oper_f_sub_v(void) { fbin(1, 1); }
void oper_f_mul_v(void) { fbin(1, 2); }
void oper_f_div_v(void) { fbin(1, 3); }
void oper_f_add_vi(void) { fbin(3, 0); }
void oper_f_sub_vi(void) { fbin(3, 1); }
void oper_f_mul_vi(void) { fbin(3, 2); }
void oper_f_div_vi(void) { fbin(3, 3); }
void oper_f_add_u8(void) { fbin(4, 0); }
void oper_f_sub_u8(void) { fbin(4, 1); }
void oper_f_mul_u8(void) { fbin(4, 2); }
void oper_f_div_u8(void) { fbin(4, 3); }
void oper_f_add_f32(void) { fbin(5, 0); }
void oper_f_sub_f32(void) { fbin(5, 1); }
void oper_f_mul_f32(void) { fbin(5, 2); }
void oper_f_div_f32(void) { fbin(5, 3); }

/* F.NEG ( f -- -f ): 0 stays 0. */
void oper_f_neg(void)
{
    if (exec.sp->u & 0x7FFFFFFF)
        exec.sp->u ^= SNGL_SIGN;
}

/* A compare's end: the relation's bit against the mask at exec.ip; a compare pushes -1 or 0, a
   CMPIF's false ends the line. */
static void ctail(uint8_t rel, bool cif)
{
    bool yes = rel & exec_u8();
    if (!cif)
        exec_pushi(yes ? -1 : 0);
    else if (!yes)
        flow_nextln();
}

static void icmp(int src, bool cif)
{
    int32_t b = (int32_t)operand(src);
    int32_t a = exec_popi();
    ctail(a > b ? OPER_GT : a == b ? OPER_EQ : OPER_LT, cif);
}

/* FLOAT compare: the left against the right. */
static void fcmp(int src, bool cif)
{
    Sngl b = operand(src);
    Sngl a = exec_pop().u;
    int r = sngl_cmp(b, a); /* the asm's flt_cmp: the right (FAC) against the left */
    ctail(r < 0 ? OPER_GT : r == 0 ? OPER_EQ : OPER_LT, cif);
}

void oper_i_cmp(void) { icmp(0, false); }
void oper_i_cmp_v(void) { icmp(1, false); }
void oper_i_cmp_u8(void) { icmp(2, false); }
void oper_i_cmpif(void) { icmp(0, true); }
void oper_i_cmpif_v(void) { icmp(1, true); }
void oper_i_cmpif_u8(void) { icmp(2, true); }
void oper_f_cmp(void) { fcmp(0, false); }
void oper_f_cmp_v(void) { fcmp(1, false); }
void oper_f_cmp_vi(void) { fcmp(3, false); }
void oper_f_cmp_u8(void) { fcmp(4, false); }
void oper_f_cmp_f32(void) { fcmp(5, false); }
void oper_f_cmpif(void) { fcmp(0, true); }
void oper_f_cmpif_v(void) { fcmp(1, true); }
void oper_f_cmpif_vi(void) { fcmp(3, true); }
void oper_f_cmpif_u8(void) { fcmp(4, true); }
void oper_f_cmpif_f32(void) { fcmp(5, true); }

/* X.IF ( x -- ): 0 ends the line. */
void oper_i_if(void)
{
    if (!exec_popi())
        flow_nextln();
}
void oper_f_if(void)
{
    if (!(exec_pop().u & 0x7FFFFFFF))
        flow_nextln();
}

void oper_i_tof(void) { exec.sp->u = fmth_from_i32(exec.sp->i); }
void oper_f_toi(void) { exec.sp->i = fmth_to_i32(exec.sp->u); }
/* F.TOB ( f -- i ): 0..255. Illegal function call. */
void oper_f_tob(void)
{
    int16_t v = fmth_to_i16(exec.sp->u);
    if ((uint16_t)v > 255)
        repl_error(E_IQ);
    exec_pop();
    exec_pushi(v);
}
/* F.TOA ( f -- i ): an address, 0..65535. */
void oper_f_toa(void)
{
    uint16_t v = fmth_to_u16(exec.sp->u);
    exec_pop();
    exec_pushi(v);
}
void oper_i_tob(void)
{
    if (exec.sp->u > 255)
        repl_error(E_IQ);
}
void oper_i_toa(void)
{
    if (exec.sp->u > 65535)
        repl_error(E_IQ);
}
void oper_drop(void) { exec.sp++; }
