/* The simple ops: loads, stores, INT and SINGLE operators, compares, IF, conversions, DROP.
 *
 * # How it works
 * An INT op works on the top cell's low dword. int_op and sngl_op do each operator once; the op's
 * variants differ only in where the right operand comes from (Source): the stack, a variable, a
 * byte, a SINGLE.
 *
 * A compare op's operand is its mask of the relations it is true for (OPER_GT, OPER_EQ, OPER_LT).
 * It pushes -1 or 0. A CMPIF's false, like a false IF, ends the line (flow_nextln).
 *
 * # Example
 *   x% + 5     I_LD_V x%, then I_ADD_U8 5
 *
 * # asm structure (src/vm/bc.asm)
 * | asm | here | |
 * |---|---|---|
 * | bc_X, every op handler | oper_x, the op's name in lower case (bc_I_ADD_V is oper_i_add_v) | MIRROR; D1 |
 * | bvar, fcall, isrc, fsrc, ibin, fbin, ctail, icmp, fcmp | var_cell, operand, int_op, sngl_op, int_cmp, sngl_cmp_op, compared | MIRROR |
 * | g_bflt, compare mask bits 1 2 4 | tables here; OPER_GT, OPER_EQ, OPER_LT | MIRROR |
 * | g_relbit (calls.asm) | compared | MOVED |
 *
 * - D1. bc_F_LD_V is oper_i_ld_v: an INT and a SINGLE load alike.
 */
#include "vm/operands.h"
#include "session.h"
#include "io/repl.h"
#include "mem/symbols.h"
#include "num/fpmath.h"
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
static Addr oper_var_cell(Vm *const vm, const Symb *const sy)
{
    return symb_cell(sy, exec_u16(vm));
}

static uint32_t oper_operand(Vm *const vm, const Symb *const sy, const Source src)
{
    switch (src) {
    case POPPED:
        return exec_pop(vm).u;

    case VAR:
        return rd32(oper_var_cell(vm, sy));

    case U8:
        return exec_u8(vm);

    case VAR_AS_SNGL:
        return fmth_from_i32((int32_t)rd32(oper_var_cell(vm, sy)));

    case U8_AS_SNGL:
        return fmth_from_i32(exec_u8(vm));

    case F32:
        return exec_u32(vm);
    }

    unreachable();
}

/* ---- loads and stores ----------------------------------------------------------------------- */

void oper_i_ld_v(Session *const S) { exec_pusha(&S->vm, oper_operand(&S->vm, &S->symb, VAR)); }
void oper_i_ld_i16(Session *const S) { exec_pusha(&S->vm, exec_u16(&S->vm)); } /* unsigned */
void oper_i_ld_i32(Session *const S) { exec_pusha(&S->vm, exec_u32(&S->vm)); }
void oper_i_ld_u8(Session *const S) { exec_pusha(&S->vm, exec_u8(&S->vm)); }
void oper_f_ld_f32(Session *const S) { exec_pusha(&S->vm, oper_operand(&S->vm, &S->symb, F32)); }
void oper_f_ld_u8(Session *const S)
{
    exec_pusha(&S->vm, oper_operand(&S->vm, &S->symb, U8_AS_SNGL));
}
void oper_f_ld_vi(Session *const S)
{
    exec_pusha(&S->vm, oper_operand(&S->vm, &S->symb, VAR_AS_SNGL));
}
void oper_f_ld_pi(Session *const S) { exec_pusha(&S->vm, SNGL_PI); }
/* A string variable: its descriptor. */
void oper_s_ld_v(Session *const S) { exec_pusha(&S->vm, oper_var_cell(&S->vm, &S->symb)); }

void oper_i_st_v(Session *const S)
{
    const Addr cell = oper_var_cell(&S->vm, &S->symb);
    wr32(cell, exec_pop(&S->vm).u);
}
void oper_f_st_v(Session *const S) { oper_i_st_v(S); }

/* ---- INT ------------------------------------------------------------------------------------ */

typedef enum { ADD, SUB, MUL, DIV, AND, OR } IntOp;

/* The top cell op the right operand. Overflow. */
static void oper_int_op(
    Vm *const vm,
    const Symb *const sy,
    Fault *const f,
    const Source src,
    const IntOp op
)
{
    const int32_t b = (int32_t)oper_operand(vm, sy, src);
    Cell *const top = vm->sp;
    int32_t r, rem;
    bool overflow = false;

    switch (op) {
    case ADD:
        overflow = __builtin_add_overflow(top->i, b, &r);
        break;

    case SUB:
        overflow = __builtin_sub_overflow(top->i, b, &r);
        break;

    case MUL:
        overflow = __builtin_mul_overflow(top->i, b, &r);
        break;

    case DIV:
        r = exec_idiv(f, top->i, b, &rem);
        break;

    case AND:
        r = top->i & b;
        break;

    case OR:
        r = top->i | b;
        break;
    }

    if (overflow)
        fault_error(f, E_OV);

    top->i = r;
}

void oper_i_add(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, POPPED, ADD); }
void oper_i_sub(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, POPPED, SUB); }
void oper_i_mul(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, POPPED, MUL); }
void oper_i_div(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, POPPED, DIV); }
void oper_i_and(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, POPPED, AND); }
void oper_i_or(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, POPPED, OR); }
void oper_i_add_v(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, VAR, ADD); }
void oper_i_sub_v(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, VAR, SUB); }
void oper_i_mul_v(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, VAR, MUL); }
void oper_i_div_v(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, VAR, DIV); }
void oper_i_and_v(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, VAR, AND); }
void oper_i_or_v(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, VAR, OR); }
void oper_i_add_u8(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, U8, ADD); }
void oper_i_sub_u8(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, U8, SUB); }
void oper_i_mul_u8(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, U8, MUL); }
void oper_i_div_u8(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, U8, DIV); }
void oper_i_and_u8(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, U8, AND); }
void oper_i_or_u8(Session *const S) { oper_int_op(&S->vm, &S->symb, &S->fault, U8, OR); }

/* MOD: the remainder takes the left operand's sign. */
void oper_i_mod(Session *const S)
{
    const int32_t b = exec_popi(&S->vm);
    int32_t rem = 0;

    if (b != -1)
        exec_idiv(&S->fault, S->vm.sp->i, b, &rem);

    S->vm.sp->i = rem;
}

void oper_i_xor(Session *const S)
{
    const int32_t b = exec_popi(&S->vm);
    S->vm.sp->i ^= b;
}

void oper_i_eqv(Session *const S)
{
    const int32_t b = exec_popi(&S->vm);
    S->vm.sp->i = ~(S->vm.sp->i ^ b);
}

void oper_i_imp(Session *const S)
{
    const int32_t b = exec_popi(&S->vm);
    S->vm.sp->i = ~S->vm.sp->i | b;
}

/* Overflow at -2^31. */
void oper_i_neg(Session *const S)
{
    if (S->vm.sp->i == INT32_MIN)
        repl_error(S, E_OV);

    S->vm.sp->i = -S->vm.sp->i;
}

void oper_i_not(Session *const S) { S->vm.sp->i = ~S->vm.sp->i; }

/* ---- SINGLE --------------------------------------------------------------------------------- */

typedef enum { FADD, FSUB, FMUL, FDIV } SnglOp;

/* The left operand popped op the right. */
static void oper_sngl_op(
    Vm *const vm,
    const Symb *const sy,
    Fault *const f,
    const Source src,
    const SnglOp op
)
{
    const Sngl b = oper_operand(vm, sy, src);
    Cell *const top = vm->sp;

    switch (op) {
    case FADD:
        top->u = fmth_add(top->u, b);
        break;

    case FSUB:
        top->u = fmth_sub(top->u, b);
        break;

    case FMUL:
        top->u = fmth_mul(top->u, b);
        break;

    case FDIV:
        top->u = fmth_div(f, top->u, b);
        break;
    }
}

void oper_f_add(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, POPPED, FADD); }
void oper_f_sub(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, POPPED, FSUB); }
void oper_f_mul(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, POPPED, FMUL); }
void oper_f_div(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, POPPED, FDIV); }
void oper_f_add_v(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, VAR, FADD); }
void oper_f_sub_v(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, VAR, FSUB); }
void oper_f_mul_v(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, VAR, FMUL); }
void oper_f_div_v(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, VAR, FDIV); }
void oper_f_add_vi(Session *const S)
{
    oper_sngl_op(&S->vm, &S->symb, &S->fault, VAR_AS_SNGL, FADD);
}
void oper_f_sub_vi(Session *const S)
{
    oper_sngl_op(&S->vm, &S->symb, &S->fault, VAR_AS_SNGL, FSUB);
}
void oper_f_mul_vi(Session *const S)
{
    oper_sngl_op(&S->vm, &S->symb, &S->fault, VAR_AS_SNGL, FMUL);
}
void oper_f_div_vi(Session *const S)
{
    oper_sngl_op(&S->vm, &S->symb, &S->fault, VAR_AS_SNGL, FDIV);
}
void oper_f_add_u8(Session *const S)
{
    oper_sngl_op(&S->vm, &S->symb, &S->fault, U8_AS_SNGL, FADD);
}
void oper_f_sub_u8(Session *const S)
{
    oper_sngl_op(&S->vm, &S->symb, &S->fault, U8_AS_SNGL, FSUB);
}
void oper_f_mul_u8(Session *const S)
{
    oper_sngl_op(&S->vm, &S->symb, &S->fault, U8_AS_SNGL, FMUL);
}
void oper_f_div_u8(Session *const S)
{
    oper_sngl_op(&S->vm, &S->symb, &S->fault, U8_AS_SNGL, FDIV);
}
void oper_f_add_f32(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, F32, FADD); }
void oper_f_sub_f32(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, F32, FSUB); }
void oper_f_mul_f32(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, F32, FMUL); }
void oper_f_div_f32(Session *const S) { oper_sngl_op(&S->vm, &S->symb, &S->fault, F32, FDIV); }

/* -0 stays 0. */
void oper_f_neg(Session *const S)
{
    if (S->vm.sp->u & ~SNGL_SIGN)
        S->vm.sp->u ^= SNGL_SIGN;
}

/* ---- compares ------------------------------------------------------------------------------- */

/* The relation's bit against the op's mask: a compare pushes -1 or 0; a CMPIF's false ends the
   line. */
static void oper_compared(Session *const S, const uint8_t rel, const bool cmpif)
{
    const bool yes = rel & exec_u8(&S->vm);

    if (!cmpif)
        exec_pushi(&S->vm, yes ? -1 : 0);
    else if (!yes)
        flow_nextln(S);
}

/* The popped INT against the right operand: OPER_GT, OPER_EQ or OPER_LT. */
static uint8_t oper_int_rel(Vm *const vm, const Symb *const sy, const Source src)
{
    const int32_t b = (int32_t)oper_operand(vm, sy, src);
    const int32_t a = exec_popi(vm);
    return a > b ? OPER_GT : a == b ? OPER_EQ : OPER_LT;
}

static uint8_t oper_sngl_rel(Vm *const vm, const Symb *const sy, const Source src)
{
    const Sngl b = oper_operand(vm, sy, src);
    const int c = sngl_cmp(exec_pop(vm).u, b);
    return c > 0 ? OPER_GT : c == 0 ? OPER_EQ : OPER_LT;
}

static void oper_int_cmp(Session *const S, const Source src, const bool cmpif)
{
    oper_compared(S, oper_int_rel(&S->vm, &S->symb, src), cmpif);
}

static void oper_sngl_cmp(Session *const S, const Source src, const bool cmpif)
{
    oper_compared(S, oper_sngl_rel(&S->vm, &S->symb, src), cmpif);
}

void oper_i_cmp(Session *const S) { oper_int_cmp(S, POPPED, false); }
void oper_i_cmp_v(Session *const S) { oper_int_cmp(S, VAR, false); }
void oper_i_cmp_u8(Session *const S) { oper_int_cmp(S, U8, false); }
void oper_i_cmpif(Session *const S) { oper_int_cmp(S, POPPED, true); }
void oper_i_cmpif_v(Session *const S) { oper_int_cmp(S, VAR, true); }
void oper_i_cmpif_u8(Session *const S) { oper_int_cmp(S, U8, true); }
void oper_f_cmp(Session *const S) { oper_sngl_cmp(S, POPPED, false); }
void oper_f_cmp_v(Session *const S) { oper_sngl_cmp(S, VAR, false); }
void oper_f_cmp_vi(Session *const S) { oper_sngl_cmp(S, VAR_AS_SNGL, false); }
void oper_f_cmp_u8(Session *const S) { oper_sngl_cmp(S, U8_AS_SNGL, false); }
void oper_f_cmp_f32(Session *const S) { oper_sngl_cmp(S, F32, false); }
void oper_f_cmpif(Session *const S) { oper_sngl_cmp(S, POPPED, true); }
void oper_f_cmpif_v(Session *const S) { oper_sngl_cmp(S, VAR, true); }
void oper_f_cmpif_vi(Session *const S) { oper_sngl_cmp(S, VAR_AS_SNGL, true); }
void oper_f_cmpif_u8(Session *const S) { oper_sngl_cmp(S, U8_AS_SNGL, true); }
void oper_f_cmpif_f32(Session *const S) { oper_sngl_cmp(S, F32, true); }

/* IF: false ends the line. A SINGLE is false at +0 and -0. */
void oper_i_if(Session *const S)
{
    if (!exec_popi(&S->vm))
        flow_nextln(S);
}

void oper_f_if(Session *const S)
{
    if (!(exec_pop(&S->vm).u & ~SNGL_SIGN))
        flow_nextln(S);
}

/* ---- conversions ---------------------------------------------------------------------------- */

void oper_i_tof(Session *const S) { S->vm.sp->u = fmth_from_i32(S->vm.sp->i); }
void oper_f_toi(Session *const S) { S->vm.sp->i = fmth_to_i32(&S->fault, S->vm.sp->u); }

/* A byte, 0..255. Illegal function call. */
void oper_f_tob(Session *const S)
{
    const int16_t v = fmth_to_i16(&S->fault, S->vm.sp->u);

    if ((uint16_t)v > 255)
        repl_error(S, E_IQ);

    exec_pop(&S->vm);
    exec_pushi(&S->vm, v);
}

/* An address, 0..65535. */
void oper_f_toa(Session *const S)
{
    const uint16_t v = fmth_to_u16(&S->fault, S->vm.sp->u);
    exec_pop(&S->vm);
    exec_pushi(&S->vm, v);
}

void oper_i_tob(Session *const S)
{
    if (S->vm.sp->u > 255)
        repl_error(S, E_IQ);
}

void oper_i_toa(Session *const S)
{
    if (S->vm.sp->u > 65535)
        repl_error(S, E_IQ);
}

void oper_drop(Session *const S) { S->vm.sp++; }
