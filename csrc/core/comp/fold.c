/* Constant folding (comp/fold.asm). */
#include "comp/fold.h"
#include "session.h"
#include "comp/comp.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "mem/symb.h"
#include "vm/exec.h"
#include "vm/flow.h"

/* Code from start to end: all FK ops, two or more. */
static bool foldable(const uint8_t *s, const uint8_t *const end)
{
    uint32_t n = 0;
    for (; s < end; s += flow_opsize(s), n++)
        if (!(exec_flags(*s) & FK))
            return false;
    return n >= 2;
}

/* The value's one load at comp.out. */
static void emit(Session *const S, const Cell v)
{
    if (S->comp.type == T_INT) {
        comp_int(S, v.u);
        return;
    }
    comp_op(S, S->comp.type == T_FLT ? O_F_LD_F32 : O_D_LD_K);
    comp_dword(S, (uint32_t)v.raw);
    if (S->comp.type == T_DBL)
        comp_dword(S, (uint32_t)(v.raw >> 32));
    S->expr.kend = S->comp.out;
}

void fold_code(Session *const S, uint8_t *const start)
{
    const uint8_t t = S->comp.type;
    if (t != T_INT && t != T_FLT && t != T_DBL)
        return;
    uint8_t *const end = S->comp.out;
    if (!foldable(start, end) || end >= S->comp.lim)
        return;
    Vm *const v = &S->vm;
    uint8_t *const ip = v->ip, *const opip = v->opip;
    Cell *const sp = v->sp;
    S->comp.folding = true;
    const bool failed = setjmp(S->comp.fold);
    if (!failed) {
        v->ip = start;
        exec_until(S, end);
    }
    const Cell value = *v->sp;
    S->comp.folding = false;
    v->ip = ip;
    v->opip = opip;
    v->sp = sp;
    if (failed)
        return;
    S->comp.out = start;
    if (S->comp.iloadend > start) /* marks inside the code go */
        S->comp.iload = S->comp.iloadend = nullptr;
    if (S->comp.cmpend > start) /* no relation left for an IF to take */
        S->comp.cmpend = nullptr;
    emit(S, value);
    if (S->expr.shend == end) /* the SHORT's class as it was */
        S->expr.shend = S->comp.out;
}

void fold_fail(Session *const S)
{
    if (S->comp.folding)
        longjmp(S->comp.fold, 1);
}

/* Op op a literal's load. */
static bool is_literal(const uint8_t op)
{
    switch (op) {
    case O_I_LD_U8:
    case O_I_LD_I16:
    case O_I_LD_I32:
    case O_F_LD_F32:
    case O_F_LD_U8:
    case O_F_LD_PI:
    case O_D_LD_K: return true;
    default: return false;
    }
}

void fold_const(Session *const S)
{
    if (S->comp.tok != TK_CONST || S->comp.tgtsubs)
        return;
    uint8_t *const start = S->comp.vat;
    fold_code(S, start);
    const uint8_t op = *start;
    if (!is_literal(op))
        return;
    const uint32_t size = flow_opsize(start);
    if (start + size != S->comp.out)
        return;
    const uint16_t id = S->comp.tgt;
    symb_varx(&S->symb, id)->kop = op;
    uint8_t *const k = symb_kval(&S->symb, id);
    for (uint32_t i = 1; i < size; i++)
        k[i - 1] = start[i];
}

/* The type a literal load op gives. */
static uint8_t literal_type(const uint8_t op)
{
    switch (op) {
    case O_D_LD_K: return T_DBL;
    case O_F_LD_F32:
    case O_F_LD_U8:
    case O_F_LD_PI: return T_FLT;
    default: return T_INT;
    }
}

bool fold_load(Session *const S, const uint16_t id)
{
    const uint8_t op = symb_varx(&S->symb, id)->kop;
    if (!op)
        return false;
    const uint8_t t = literal_type(op);
    S->comp.type = t;
    if (t == T_INT)
        S->comp.iload = S->comp.out;
    const uint8_t buf[1] = {op};
    const uint32_t size = flow_opsize(buf);
    comp_op(S, op);
    const uint8_t *const k = symb_kval(&S->symb, id);
    for (uint32_t i = 1; i < size; i++)
        comp_byte(S, k[i - 1]);
    if (t == T_INT)
        S->comp.iloadend = S->comp.out;
    S->expr.kend = S->comp.out;
    expr_shortid(S, id);
    return true;
}

void fold_clear(Session *const S)
{
    for (uint16_t id = 0; id < S->symb.nvars; id++)
        symb_varx(&S->symb, id)->kop = 0;
}
