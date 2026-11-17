/* Constant folding: code made only of constant ops runs at compile time and becomes one load.
 *
 * # How it works
 * Code of FK ops only (vm/ops.def) runs as it compiles, through the VM's own handlers, so its value
 * is the run's to the bit, and becomes one load of it. fold_code runs the code with exec_until up
 * to comp.out; no FOLD op is executed. An error while it runs, caught (fault.h), leaves the code as
 * it was, to raise when run.
 *
 * A CONST whose value folds to one literal load keeps that load (VarX.kop, symb_kval): a use
 * compiled after it loads the literal instead of the variable. The CONST still stores the variable,
 * for the uses compiled before. fold_tree folds a tree's operators whose operands are literals.
 *
 * # Example
 *    CONST k = 2 * 3     k is 6: a later PRINT k loads the literal 6
 *
 * # asm structure (retired/asm32/comp/fold.asm)
 * | asm | here | |
 * |---|---|---|
 * | cm_fold, op_fold | fold_code | MIRROR; D1 |
 * | cf_fail, g_cfold, g_cfsp | run's Catch | MIRROR |
 * | cm_krec, cm_klit | fold_const | MIRROR |
 * | var_kclr | fold_clear | MIRROR |
 * | g_cvat | comp.vat | MIRROR |
 * | ctok, cleared in cm_tok | comp.tok = 0 in token (comp.c) | MOVED |
 * | FK, TVX.kop, kval | FK (ops.def), VarX.kop, symb_kval | MIRROR |
 * | (new) | fold_tree, fold_isliteral, fold_littype | D2 |
 *
 * - D1. The asm's FOLD op is never executed; the code runs through exec_until.
 * - D2. The tree is folded before codegen, operator by operator.
 */
#include "comp/fold.h"
#include "comp/ast.h"
#include "comp/comp.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "mem/symbols.h"
#include "vm/exec.h"
#include "vm/flow.h"

/* Code from start to end: all FK ops, least or more. */
static bool pure(const uint8_t *s, const uint8_t *const end, const uint32_t least)
{
    uint32_t n = 0;

    for (; s < end; s += flow_opsize(s), n++)
        if (!(exec_flags(*s) & FK))
            return false;

    return n >= least;
}

/* The value's one load at comp.out. */
static void emit(Comp *const C, const Cell v)
{
    if (C->code->type == T_INT) {
        code_int(C->code, v.u);
        return;
    }

    code_op(C->code, C->code->type == T_FLT ? O_F_LD_F32 : O_D_LD_K);
    code_dword(C->code, (uint32_t)v.raw);

    if (C->code->type == T_DBL)
        code_dword(C->code, (uint32_t)(v.raw >> 32));
}

/* The code from start to comp.out run: its value to *v; false: fewer than least ops, not all pure,
   or it failed. */
static bool run(Comp *const C, uint8_t *const start, const uint32_t least, Cell *const v)
{
    uint8_t *const end = C->code->out;

    if (!pure(start, end, least) || end >= C->code->lim)
        return false;

    Vm *const vm = C->vm;
    uint8_t *const ip = vm->ip, *const opip = vm->opip;
    Cell *const sp = vm->sp;
    Catch c;
    fault_push(C->fault, &c);
    const bool failed = setjmp(c.jb);

    if (!failed) {
        vm->ip = start;
        C->run->until(C->run, end);
        *v = *vm->sp;
        fault_pop(C->fault);
    }

    vm->ip = ip;
    vm->opip = opip;
    vm->sp = sp;
    return !failed;
}

void fold_code(Comp *const C, uint8_t *const start)
{
    const uint8_t t = C->code->type;

    if (t != T_INT && t != T_FLT && t != T_DBL)
        return;

    uint8_t *const end = C->code->out;
    Cell value;

    if (!run(C, start, 2, &value))
        return;

    C->code->out = start;

    if (C->code->iloadend > start) /* marks inside the code go */
        C->code->iload = C->code->iloadend = nullptr;
    if (C->code->cmpend > start) /* no relation left for an IF to take */
        C->code->cmpend = nullptr;

    emit(C, value);

    if (C->code->shend == end) /* the SHORT's class as it was */
        C->code->shend = C->code->out;
}

bool fold_isliteral(const uint8_t op)
{
    switch (op) {
    case O_I_LD_U8:
    case O_I_LD_I16:
    case O_I_LD_I32:
    case O_F_LD_F32:
    case O_F_LD_U8:
    case O_F_LD_PI:
    case O_D_LD_K:
        return true;

    default:
        return false;
    }
}

void fold_const(Comp *const C, uint8_t *const start, const uint16_t id)
{
    fold_code(C, start);
    const uint8_t op = *start;

    if (!fold_isliteral(op))
        return;

    const uint32_t size = flow_opsize(start);

    if (start + size != C->code->out)
        return;

    symb_varx(C->symb, id)->kop = op;
    uint8_t *const k = symb_kval(C->symb, id);

    for (uint32_t i = 1; i < size; i++)
        k[i - 1] = start[i];
}

uint8_t fold_littype(const uint8_t op)
{
    switch (op) {
    case O_D_LD_K:
        return T_DBL;

    case O_S_LD_TXT:
        return T_STR;

    case O_F_LD_F32:
    case O_F_LD_U8:
    case O_F_LD_PI:
        return T_FLT;

    default:
        return T_INT;
    }
}

/* Node n the literal load of value v, of type t. */
static void set_literal(Node *const n, const uint8_t t, const Cell v)
{
    uint8_t op;

    switch (t) {
    case T_INT:
        op = v.u > 0xFFFF ? O_I_LD_I32 : v.u > 0xFF ? O_I_LD_I16 : O_I_LD_U8;
        break;

    case T_FLT:
        op = O_F_LD_F32;
        break;

    default:
        op = O_D_LD_K;
    }

    n->kind = N_LIT;
    n->op = op;
    n->type = t;
    store32(n->lit, (uint32_t)v.raw);
    store32(n->lit + 4, (uint32_t)(v.raw >> 32));
}

static bool literal(Comp *const C, Node *const i) { return i->kind == N_LIT; }

void fold_tree(Comp *const C, Node *const i)
{
    Node *const n = i;

    if (n->kind != N_UN && n->kind != N_BIN) { /* its children: an operand, the arguments */
        for (Node *a = n->a; a != nullptr; a = a->next)
            fold_tree(C, a);
        for (Node *a = n->b; a != nullptr; a = a->next)
            fold_tree(C, a);

        return;
    }

    fold_tree(C, n->a);

    if (n->kind == N_BIN)
        fold_tree(C, n->b);
    if (!literal(C, n->a) || (n->kind == N_BIN && !literal(C, n->b)))
        return;

    uint8_t *const start = C->code->out;
    cg_tree(C->code, i);
    const uint8_t t = C->code->type, sh = code_shclass(C->code);
    Cell v;
    const bool ok = (t == T_INT || t == T_FLT || t == T_DBL) && run(C, start, 2, &v);
    C->code->out = start;
    C->code->iload = C->code->iloadend = C->code->cmp = C->code->cmpend = nullptr;

    if (!ok)
        return;

    set_literal(n, t, v);
    n->sh = sh;
}

bool fold_number(Comp *const C, const Node *const n, Cell *const v)
{
    uint8_t *const start = C->code->out;
    cg_tree(C->code, n);
    const uint8_t t = C->code->type;
    const bool ok = (t == T_INT || t == T_FLT) && run(C, start, 1, v);
    C->code->out = start;
    C->code->iload = C->code->iloadend = C->code->cmp = C->code->cmpend = nullptr;
    return ok;
}

void fold_clear(Comp *const C)
{
    for (uint16_t id = 0; id < C->symb->nvars; id++)
        symb_varx(C->symb, id)->kop = 0;
}
