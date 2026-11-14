/* Constant folding (comp/fold.asm). */
#include "comp/fold.h"
#include "comp/ast.h"
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
}

/* The code from start to comp.out run: its value to *v; false: not foldable, or it failed. */
static bool eval(Session *const S, uint8_t *const start, Cell *const v)
{
    uint8_t *const end = S->comp.out;
    if (!foldable(start, end) || end >= S->comp.lim)
        return false;
    Vm *const vm = &S->vm;
    uint8_t *const ip = vm->ip, *const opip = vm->opip;
    Cell *const sp = vm->sp;
    S->comp.folding = true;
    const bool failed = setjmp(S->comp.fold);
    if (!failed) {
        vm->ip = start;
        exec_until(S, end);
        *v = *vm->sp;
    }
    S->comp.folding = false;
    vm->ip = ip;
    vm->opip = opip;
    vm->sp = sp;
    return !failed;
}

void fold_code(Session *const S, uint8_t *const start)
{
    const uint8_t t = S->comp.type;
    if (t != T_INT && t != T_FLT && t != T_DBL)
        return;
    uint8_t *const end = S->comp.out;
    Cell value;
    if (!eval(S, start, &value))
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

bool fold_isliteral(const uint8_t op)
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
    if (!fold_isliteral(op))
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

uint8_t fold_littype(const uint8_t op)
{
    switch (op) {
    case O_D_LD_K: return T_DBL;
    case O_S_LD_TXT: return T_STR;
    case O_F_LD_F32:
    case O_F_LD_U8:
    case O_F_LD_PI: return T_FLT;
    default: return T_INT;
    }
}

/* Node n the literal load of value v, of type t. */
static void set_literal(Node *const n, const uint8_t t, const Cell v)
{
    uint8_t op;
    switch (t) {
    case T_INT: op = v.u > 0xFFFF ? O_I_LD_I32 : v.u > 0xFF ? O_I_LD_I16 : O_I_LD_U8; break;
    case T_FLT: op = O_F_LD_F32; break;
    default: op = O_D_LD_K;
    }
    n->kind = N_LIT;
    n->op = op;
    n->type = t;
    store32(n->lit, (uint32_t)v.raw);
    store32(n->lit + 4, (uint32_t)(v.raw >> 32));
}

static bool literal(Session *const S, const uint16_t i) { return S->ast.node[i].kind == N_LIT; }

void fold_tree(Session *const S, const uint16_t i)
{
    Node *const n = &S->ast.node[i];
    switch (n->kind) {
    case N_ELEM:
    case N_FN:
    case N_XFN:
    case N_REF:
    case N_CALL: /* its arguments */
        for (uint16_t a = n->b; a != NO_NODE; a = S->ast.node[a].next)
            fold_tree(S, a);
        return;
    case N_FIELD:
    case N_ADDR:
    case N_ARG:
        if (n->a != NO_NODE)
            fold_tree(S, n->a);
        return;
    default: break;
    }
    if (n->kind != N_UN && n->kind != N_BIN)
        return;
    fold_tree(S, n->a);
    if (n->kind == N_BIN)
        fold_tree(S, n->b);
    if (!literal(S, n->a) || (n->kind == N_BIN && !literal(S, n->b)))
        return;
    uint8_t *const start = S->comp.out;
    expr_gen(S, i);
    const uint8_t t = S->comp.type, sh = expr_shclass(S);
    Cell v;
    const bool ok = (t == T_INT || t == T_FLT || t == T_DBL) && eval(S, start, &v);
    S->comp.out = start;
    S->comp.iload = S->comp.iloadend = S->comp.cmp = S->comp.cmpend = nullptr;
    if (!ok)
        return;
    set_literal(n, t, v);
    n->sh = sh;
}

void fold_clear(Session *const S)
{
    for (uint16_t id = 0; id < S->symb.nvars; id++)
        symb_varx(&S->symb, id)->kop = 0;
}
