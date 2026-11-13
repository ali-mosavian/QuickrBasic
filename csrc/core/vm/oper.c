/* The simple ops: loads, stores, INT and SINGLE operators, compares, IF, conversions, DROP. An INT
   op works on the top cell's low dword. */
#include "vm/oper.h"
#include "io/repl.h"
#include "mem/symb.h"
#include "num/fmth.h"
#include "vm/flow.h"

/* Where an op's right operand comes from. */
typedef enum {
    POPPED,      /* the data stack */
    VAR,         /* the variable whose id follows the op */
    U8,          /* the byte that follows, an INT */
    VAR_AS_SNGL, /* an INT variable, as a SINGLE */
    U8_AS_SNGL,  /* the byte that follows, as a SINGLE */
    F32,         /* the SINGLE that follows */
} Source;

/* The cell of the variable whose id is at vm->ip. */
static Addr var_cell(Vm *const vm) { return symb_cell(exec_u16(vm)); }

static uint32_t operand(Vm *const vm, const Source src)
{
    switch (src) {
    case POPPED: return exec_pop(vm).u;
    case VAR: return rd32(var_cell(vm));
    case U8: return exec_u8(vm);
    case VAR_AS_SNGL: return fmth_from_i32((int32_t)rd32(var_cell(vm)));
    case U8_AS_SNGL: return fmth_from_i32(exec_u8(vm));
    case F32: return exec_u32(vm);
    }
    unreachable();
}

/* ---- loads and stores ----------------------------------------------------------------------- */

void oper_i_ld_v(Vm *const vm) { exec_pusha(vm, operand(vm, VAR)); }
void oper_i_ld_i16(Vm *const vm) { exec_pusha(vm, exec_u16(vm)); } /* unsigned */
void oper_i_ld_i32(Vm *const vm) { exec_pusha(vm, exec_u32(vm)); }
void oper_i_ld_u8(Vm *const vm) { exec_pusha(vm, exec_u8(vm)); }
void oper_f_ld_f32(Vm *const vm) { exec_pusha(vm, operand(vm, F32)); }
void oper_f_ld_u8(Vm *const vm) { exec_pusha(vm, operand(vm, U8_AS_SNGL)); }
void oper_f_ld_vi(Vm *const vm) { exec_pusha(vm, operand(vm, VAR_AS_SNGL)); }
void oper_f_ld_pi(Vm *const vm) { exec_pusha(vm, SNGL_PI); }
/* A string variable: its descriptor. */
void oper_s_ld_v(Vm *const vm) { exec_pusha(vm, var_cell(vm)); }

void oper_i_st_v(Vm *const vm)
{
    const Addr cell = var_cell(vm);
    wr32(cell, exec_pop(vm).u);
}
void oper_f_st_v(Vm *const vm) { oper_i_st_v(vm); }

/* ---- INT ------------------------------------------------------------------------------------ */

typedef enum { ADD, SUB, MUL, DIV, AND, OR } IntOp;

/* The top cell op the right operand. Overflow. */
static void int_op(Vm *const vm, const Source src, const IntOp op)
{
    const int32_t b = (int32_t)operand(vm, src);
    Cell *const top = vm->sp;
    int32_t r, rem;
    bool overflow = false;
    switch (op) {
    case ADD: overflow = __builtin_add_overflow(top->i, b, &r); break;
    case SUB: overflow = __builtin_sub_overflow(top->i, b, &r); break;
    case MUL: overflow = __builtin_mul_overflow(top->i, b, &r); break;
    case DIV: r = exec_idiv(top->i, b, &rem); break;
    case AND: r = top->i & b; break;
    case OR: r = top->i | b; break;
    }
    if (overflow)
        repl_error(E_OV);
    top->i = r;
}

void oper_i_add(Vm *const vm) { int_op(vm, POPPED, ADD); }
void oper_i_sub(Vm *const vm) { int_op(vm, POPPED, SUB); }
void oper_i_mul(Vm *const vm) { int_op(vm, POPPED, MUL); }
void oper_i_div(Vm *const vm) { int_op(vm, POPPED, DIV); }
void oper_i_and(Vm *const vm) { int_op(vm, POPPED, AND); }
void oper_i_or(Vm *const vm) { int_op(vm, POPPED, OR); }
void oper_i_add_v(Vm *const vm) { int_op(vm, VAR, ADD); }
void oper_i_sub_v(Vm *const vm) { int_op(vm, VAR, SUB); }
void oper_i_mul_v(Vm *const vm) { int_op(vm, VAR, MUL); }
void oper_i_div_v(Vm *const vm) { int_op(vm, VAR, DIV); }
void oper_i_and_v(Vm *const vm) { int_op(vm, VAR, AND); }
void oper_i_or_v(Vm *const vm) { int_op(vm, VAR, OR); }
void oper_i_add_u8(Vm *const vm) { int_op(vm, U8, ADD); }
void oper_i_sub_u8(Vm *const vm) { int_op(vm, U8, SUB); }
void oper_i_mul_u8(Vm *const vm) { int_op(vm, U8, MUL); }
void oper_i_div_u8(Vm *const vm) { int_op(vm, U8, DIV); }
void oper_i_and_u8(Vm *const vm) { int_op(vm, U8, AND); }
void oper_i_or_u8(Vm *const vm) { int_op(vm, U8, OR); }

/* MOD: the remainder takes the left operand's sign. */
void oper_i_mod(Vm *const vm)
{
    const int32_t b = exec_popi(vm);
    int32_t rem = 0;
    if (b != -1)
        exec_idiv(vm->sp->i, b, &rem);
    vm->sp->i = rem;
}

void oper_i_xor(Vm *const vm)
{
    const int32_t b = exec_popi(vm);
    vm->sp->i ^= b;
}

void oper_i_eqv(Vm *const vm)
{
    const int32_t b = exec_popi(vm);
    vm->sp->i = ~(vm->sp->i ^ b);
}

void oper_i_imp(Vm *const vm)
{
    const int32_t b = exec_popi(vm);
    vm->sp->i = ~vm->sp->i | b;
}

/* Overflow at -2^31. */
void oper_i_neg(Vm *const vm)
{
    if (vm->sp->i == INT32_MIN)
        repl_error(E_OV);
    vm->sp->i = -vm->sp->i;
}

void oper_i_not(Vm *const vm) { vm->sp->i = ~vm->sp->i; }

/* ---- SINGLE --------------------------------------------------------------------------------- */

typedef enum { FADD, FSUB, FMUL, FDIV } SnglOp;

/* The left operand popped op the right. */
static void sngl_op(Vm *const vm, const Source src, const SnglOp op)
{
    const Sngl b = operand(vm, src);
    Cell *const top = vm->sp;
    switch (op) {
    case FADD: top->u = fmth_add(top->u, b); break;
    case FSUB: top->u = fmth_sub(top->u, b); break;
    case FMUL: top->u = fmth_mul(top->u, b); break;
    case FDIV: top->u = fmth_div(top->u, b); break;
    }
}

void oper_f_add(Vm *const vm) { sngl_op(vm, POPPED, FADD); }
void oper_f_sub(Vm *const vm) { sngl_op(vm, POPPED, FSUB); }
void oper_f_mul(Vm *const vm) { sngl_op(vm, POPPED, FMUL); }
void oper_f_div(Vm *const vm) { sngl_op(vm, POPPED, FDIV); }
void oper_f_add_v(Vm *const vm) { sngl_op(vm, VAR, FADD); }
void oper_f_sub_v(Vm *const vm) { sngl_op(vm, VAR, FSUB); }
void oper_f_mul_v(Vm *const vm) { sngl_op(vm, VAR, FMUL); }
void oper_f_div_v(Vm *const vm) { sngl_op(vm, VAR, FDIV); }
void oper_f_add_vi(Vm *const vm) { sngl_op(vm, VAR_AS_SNGL, FADD); }
void oper_f_sub_vi(Vm *const vm) { sngl_op(vm, VAR_AS_SNGL, FSUB); }
void oper_f_mul_vi(Vm *const vm) { sngl_op(vm, VAR_AS_SNGL, FMUL); }
void oper_f_div_vi(Vm *const vm) { sngl_op(vm, VAR_AS_SNGL, FDIV); }
void oper_f_add_u8(Vm *const vm) { sngl_op(vm, U8_AS_SNGL, FADD); }
void oper_f_sub_u8(Vm *const vm) { sngl_op(vm, U8_AS_SNGL, FSUB); }
void oper_f_mul_u8(Vm *const vm) { sngl_op(vm, U8_AS_SNGL, FMUL); }
void oper_f_div_u8(Vm *const vm) { sngl_op(vm, U8_AS_SNGL, FDIV); }
void oper_f_add_f32(Vm *const vm) { sngl_op(vm, F32, FADD); }
void oper_f_sub_f32(Vm *const vm) { sngl_op(vm, F32, FSUB); }
void oper_f_mul_f32(Vm *const vm) { sngl_op(vm, F32, FMUL); }
void oper_f_div_f32(Vm *const vm) { sngl_op(vm, F32, FDIV); }

/* -0 stays 0. */
void oper_f_neg(Vm *const vm)
{
    if (vm->sp->u & ~SNGL_SIGN)
        vm->sp->u ^= SNGL_SIGN;
}

/* ---- compares ------------------------------------------------------------------------------- */

/* The relation's bit against the op's mask: a compare pushes -1 or 0; a CMPIF's false ends the
   line. */
static void compared(Vm *const vm, const uint8_t rel, const bool cmpif)
{
    const bool yes = rel & exec_u8(vm);
    if (!cmpif)
        exec_pushi(vm, yes ? -1 : 0);
    else if (!yes)
        flow_nextln(vm);
}

static void int_cmp(Vm *const vm, const Source src, const bool cmpif)
{
    const int32_t b = (int32_t)operand(vm, src);
    const int32_t a = exec_popi(vm);
    compared(vm, a > b ? OPER_GT : a == b ? OPER_EQ : OPER_LT, cmpif);
}

static void sngl_cmp_op(Vm *const vm, const Source src, const bool cmpif)
{
    const Sngl b = operand(vm, src);
    const int c = sngl_cmp(exec_pop(vm).u, b);
    compared(vm, c > 0 ? OPER_GT : c == 0 ? OPER_EQ : OPER_LT, cmpif);
}

void oper_i_cmp(Vm *const vm) { int_cmp(vm, POPPED, false); }
void oper_i_cmp_v(Vm *const vm) { int_cmp(vm, VAR, false); }
void oper_i_cmp_u8(Vm *const vm) { int_cmp(vm, U8, false); }
void oper_i_cmpif(Vm *const vm) { int_cmp(vm, POPPED, true); }
void oper_i_cmpif_v(Vm *const vm) { int_cmp(vm, VAR, true); }
void oper_i_cmpif_u8(Vm *const vm) { int_cmp(vm, U8, true); }
void oper_f_cmp(Vm *const vm) { sngl_cmp_op(vm, POPPED, false); }
void oper_f_cmp_v(Vm *const vm) { sngl_cmp_op(vm, VAR, false); }
void oper_f_cmp_vi(Vm *const vm) { sngl_cmp_op(vm, VAR_AS_SNGL, false); }
void oper_f_cmp_u8(Vm *const vm) { sngl_cmp_op(vm, U8_AS_SNGL, false); }
void oper_f_cmp_f32(Vm *const vm) { sngl_cmp_op(vm, F32, false); }
void oper_f_cmpif(Vm *const vm) { sngl_cmp_op(vm, POPPED, true); }
void oper_f_cmpif_v(Vm *const vm) { sngl_cmp_op(vm, VAR, true); }
void oper_f_cmpif_vi(Vm *const vm) { sngl_cmp_op(vm, VAR_AS_SNGL, true); }
void oper_f_cmpif_u8(Vm *const vm) { sngl_cmp_op(vm, U8_AS_SNGL, true); }
void oper_f_cmpif_f32(Vm *const vm) { sngl_cmp_op(vm, F32, true); }

/* IF: false ends the line. A SINGLE is false at +0 and -0. */
void oper_i_if(Vm *const vm)
{
    if (!exec_popi(vm))
        flow_nextln(vm);
}

void oper_f_if(Vm *const vm)
{
    if (!(exec_pop(vm).u & ~SNGL_SIGN))
        flow_nextln(vm);
}

/* ---- conversions ---------------------------------------------------------------------------- */

void oper_i_tof(Vm *const vm) { vm->sp->u = fmth_from_i32(vm->sp->i); }
void oper_f_toi(Vm *const vm) { vm->sp->i = fmth_to_i32(vm->sp->u); }

/* A byte, 0..255. Illegal function call. */
void oper_f_tob(Vm *const vm)
{
    const int16_t v = fmth_to_i16(vm->sp->u);
    if ((uint16_t)v > 255)
        repl_error(E_IQ);
    exec_pop(vm);
    exec_pushi(vm, v);
}

/* An address, 0..65535. */
void oper_f_toa(Vm *const vm)
{
    const uint16_t v = fmth_to_u16(vm->sp->u);
    exec_pop(vm);
    exec_pushi(vm, v);
}

void oper_i_tob(Vm *const vm)
{
    if (vm->sp->u > 255)
        repl_error(E_IQ);
}

void oper_i_toa(Vm *const vm)
{
    if (vm->sp->u > 65535)
        repl_error(E_IQ);
}

void oper_drop(Vm *const vm) { vm->sp++; }
